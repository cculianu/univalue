// Copyright 2014 BitPay Inc.
// Copyright (c) 2020-2021 The Bitcoin developers
// Copyright (c) 2021 Calin A. Culianu <calin.culianu@gmail.com>
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "univalue.h"
#include "univalue_internal.h"

#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

/**
 * According to stackexchange, the original json test suite wanted
 * to limit depth to 22.  Widely-deployed PHP bails at depth 512,
 * so we will follow PHP's lead, which should be more than sufficient
 * (further stackexchange comments indicate depth > 32 rarely occurs).
 */
inline constexpr size_t MAX_JSON_DEPTH = 512;

inline constexpr bool json_isdigit(char ch) noexcept
{
    return ch >= '0' && ch <= '9';
}

// Attention: in several functions below, you are reading a NUL-terminated string.
// Always assure yourself that a character is not NUL, prior to advancing the pointer to the next character.

/**
 * Helper for getJsonToken; converts hexadecimal string to unsigned integer.
 *
 * Returns a nullopt if conversion fails due to not enough characters (requires
 * minimum 4 characters), or if any of the characters encountered are not hex.
 *
 * On success, returns an optional containing the converted codepoint.
 *
 * Consumes the first 4 hex characters in `buffer`, insofar they exist.
 */
inline constexpr std::optional<unsigned> hatoui(const char*& buffer) noexcept
{
    unsigned val = 0;
    for (const char* end = buffer + 4; buffer != end; ++buffer) {  // consume 4 chars from buffer
        val *= 16;
        if (json_isdigit(*buffer))
            val += *buffer - '0';
        else if (*buffer >= 'a' && *buffer <= 'f')
            val += *buffer - 'a' + 10;
        else if (*buffer >= 'A' && *buffer <= 'F')
            val += *buffer - 'A' + 10;
        else
            return std::nullopt; // not a hex digit, fail
    }
    return val;
}

/**
 * Filter that generates and validates UTF-8, as well as collates UTF-16
 * surrogate pairs as specified in RFC4627.
 */
class JSONUTF8StringFilter
{
public:
    explicit JSONUTF8StringFilter(std::string &s) : str(s) {}
    // Write single 8-bit char (may be part of UTF-8 sequence)
    void push_back(unsigned char ch)
    {
        if (state == 0) {
            if (ch < 0x80) // 7-bit ASCII, fast direct pass-through
                str.push_back(ch);
            else if (ch < 0xc0) // Mid-sequence character, invalid in this state
                is_valid = false;
            else if (ch < 0xe0) { // Start of 2-byte sequence
                codepoint = (ch & 0x1f) << 6;
                state = 6;
            } else if (ch < 0xf0) { // Start of 3-byte sequence
                codepoint = (ch & 0x0f) << 12;
                state = 12;
            } else if (ch < 0xf8) { // Start of 4-byte sequence
                codepoint = (ch & 0x07) << 18;
                state = 18;
            } else // Reserved, invalid
                is_valid = false;
        } else {
            if ((ch & 0xc0) != 0x80) // Not a continuation, invalid
                is_valid = false;
            state -= 6;
            codepoint |= (ch & 0x3f) << state;
            if (state == 0)
                push_back_u(codepoint);
        }
    }
    // Write codepoint directly, possibly collating surrogate pairs
    void push_back_u(unsigned int codepoint_)
    {
        if (state) // Only accept full codepoints in open state
            is_valid = false;
        if (codepoint_ >= 0xD800 && codepoint_ < 0xDC00) { // First half of surrogate pair
            if (surpair) // Two subsequent surrogate pair openers - fail
                is_valid = false;
            else
                surpair = codepoint_;
        } else if (codepoint_ >= 0xDC00 && codepoint_ < 0xE000) { // Second half of surrogate pair
            if (surpair) { // Open surrogate pair, expect second half
                // Compute code point from UTF-16 surrogate pair
                append_codepoint(0x10000 | ((surpair - 0xD800)<<10) | (codepoint_ - 0xDC00));
                surpair = 0;
            } else // Second half doesn't follow a first half - fail
                is_valid = false;
        } else {
            if (surpair) // First half of surrogate pair not followed by second - fail
                is_valid = false;
            else
                append_codepoint(codepoint_);
        }
    }
    // Check that we're in a state where the string can be ended
    // No open sequences, no open surrogate pairs, etc
    bool finalize() noexcept
    {
        if (state || surpair)
            is_valid = false;
        return is_valid;
    }
private:
    std::string &str;
    bool is_valid = true;
    // Current UTF-8 decoding state
    unsigned int codepoint = 0;
    int state = 0; // Top bit to be filled in for next UTF-8 byte, or 0

    // Keep track of the following state to handle the following section of
    // RFC4627:
    //
    //    To escape an extended character that is not in the Basic Multilingual
    //    Plane, the character is represented as a twelve-character sequence,
    //    encoding the UTF-16 surrogate pair.  So, for example, a string
    //    containing only the G clef character (U+1D11E) may be represented as
    //    "\uD834\uDD1E".
    //
    //  Two subsequent \u.... may have to be replaced with one actual codepoint.
    unsigned int surpair = 0; // First half of open UTF-16 surrogate pair, or 0

    void append_codepoint(unsigned int codepoint_)
    {
        if (codepoint_ <= 0x7f)
            str.push_back((char)codepoint_);
        else if (codepoint_ <= 0x7FF) {
            str.push_back((char)(0xC0 | (codepoint_ >> 6)));
            str.push_back((char)(0x80 | (codepoint_ & 0x3F)));
        } else if (codepoint_ <= 0xFFFF) {
            str.push_back((char)(0xE0 | (codepoint_ >> 12)));
            str.push_back((char)(0x80 | ((codepoint_ >> 6) & 0x3F)));
            str.push_back((char)(0x80 | (codepoint_ & 0x3F)));
        } else if (codepoint_ <= 0x1FFFFF) {
            str.push_back((char)(0xF0 | (codepoint_ >> 18)));
            str.push_back((char)(0x80 | ((codepoint_ >> 12) & 0x3F)));
            str.push_back((char)(0x80 | ((codepoint_ >> 6) & 0x3F)));
            str.push_back((char)(0x80 | (codepoint_ & 0x3F)));
        }
    }
};

enum jtokentype {
    JTOK_ERR        = -1,
    JTOK_NONE       = 0,                           // eof
    JTOK_OBJ_OPEN,
    JTOK_OBJ_CLOSE,
    JTOK_ARR_OPEN,
    JTOK_ARR_CLOSE,
    JTOK_COLON,
    JTOK_COMMA,
    JTOK_KW_NULL,
    JTOK_KW_TRUE,
    JTOK_KW_FALSE,
    JTOK_NUMBER,
    JTOK_STRING,
};

[[nodiscard]]
inline constexpr bool jsonTokenIsValue(jtokentype jtt) noexcept
{
    switch (jtt) {
    case JTOK_KW_NULL:
    case JTOK_KW_TRUE:
    case JTOK_KW_FALSE:
    case JTOK_NUMBER:
    case JTOK_STRING:
        return true;

    default:
        return false;
    }

    // not reached
}

[[nodiscard]]
inline constexpr bool json_isspace(int ch) noexcept
{
    switch (ch) {
    case 0x20:
    case 0x09:
    case 0x0a:
    case 0x0d:
        return true;

    default:
        return false;
    }

    // not reached
}

jtokentype getJsonToken(std::string& tokenVal, const char*& buffer)
{
    tokenVal.clear();

    while (json_isspace(*buffer))          // skip whitespace
        ++buffer;

    switch (*buffer) {

    case '\0': // terminating NUL
        return JTOK_NONE;

    case '{':
        ++buffer;
        return JTOK_OBJ_OPEN;
    case '}':
        ++buffer;
        return JTOK_OBJ_CLOSE;
    case '[':
        ++buffer;
        return JTOK_ARR_OPEN;
    case ']':
        ++buffer;
        return JTOK_ARR_CLOSE;

    case ':':
        ++buffer;
        return JTOK_COLON;
    case ',':
        ++buffer;
        return JTOK_COMMA;

    case 'n': // null
        if (*++buffer == 'u' && *++buffer == 'l' && *++buffer == 'l') {
            ++buffer;
            return JTOK_KW_NULL;
        }
        return JTOK_ERR;
    case 't': // true
        if (*++buffer == 'r' && *++buffer == 'u' && *++buffer == 'e') {
            ++buffer;
            return JTOK_KW_TRUE;
        }
        return JTOK_ERR;
    case 'f': // false
        if (*++buffer == 'a' && *++buffer == 'l' && *++buffer == 's' && *++buffer == 'e') {
            ++buffer;
            return JTOK_KW_FALSE;
        }
        return JTOK_ERR;

    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9': {
        // part 1: int
        const char * const first = buffer;
        const bool firstIsMinus = *first == '-';

        const char * const firstDigit = first + firstIsMinus;

        if (*firstDigit == '0' && json_isdigit(firstDigit[1]))
            return JTOK_ERR;

        ++buffer;                                                       // consume first char

        if (firstIsMinus && !json_isdigit(*buffer)) {
            // reject buffers ending in '-' or '-' followed by non-digit
            return JTOK_ERR;
        }

        while (json_isdigit(*buffer)) {                      // consume digits
            ++buffer;
        }

        // part 2: frac
        if (*buffer == '.') {
            ++buffer;                                                   // consume .

            if (!json_isdigit(*buffer))
                return JTOK_ERR;
            do {                                                                       // consume digits
                ++buffer;
            } while (json_isdigit(*buffer));
        }

        // part 3: exp
        if (*buffer == 'e' || *buffer == 'E') {
            ++buffer;                                                   // consume E

            if (*buffer == '-' || *buffer == '+') { // consume +/-
                ++buffer;
            }

            if (!json_isdigit(*buffer))
                return JTOK_ERR;
            do {                                                                       // consume digits
                ++buffer;
            } while (json_isdigit(*buffer));
        }

        tokenVal.assign(first, buffer);
        return JTOK_NUMBER;
        }

    case '"': {
        constexpr size_t reserveSize = 0; // set to 0 to not pre-alloc anything
        ++buffer;  // skip "

        // First, try the fast path which doesn't use the (slow) JSONUTF8StringFilter.
        // This is a common-case optimization: we optimistically scan to ensure string
        // is a simple ascii string with no unicode and no escapes, and if so, return it.
        // If we do encounter non-ascii or escapes, we accept the partial string into
        // `tokenVal`, then we proceed to the slow path.  In most real-world JSON, the
        // fast path below is the only path taken and is the common-case.
        constexpr bool tryFastPath = true;
        if constexpr (tryFastPath) {
            enum class FastPath {
                Processed, NotFullyProcessed, Error
            };

            static const auto FastPathParseSimpleString = [](const char *& raw) -> FastPath {
                for (; *raw; ++raw) {
                    const uint8_t ch = static_cast<uint8_t>(*raw);
                    if (ch == '"') {
                        // fast-path accept case: simple string end at " char
                        return FastPath::Processed;
                    } else if (ch == '\\') {
                        // has escapes -- cannot process as simple string, must continue using slow path
                        return FastPath::NotFullyProcessed;
                    } else if (ch < 0x20) {
                        // is not legal JSON because < 0x20
                        return FastPath::Error;
                    } else if (ch >= 0x80) {
                        // has a funky unicode character.. must take slow path
                        return FastPath::NotFullyProcessed;
                    }
                }
                // premature string end
                return FastPath::Error;
            };
            switch (const char * const begin = buffer; FastPathParseSimpleString(buffer /*pass-by-ref*/)) {
            case FastPath::Processed:
                // fast path taken -- the string had no embedded escapes or non-ascii characters, return
                // early, set tokenVal, set consumed. Note: raw now points to trailing " char
                assert(*buffer == '"');
                tokenVal.assign(begin, buffer);
                ++buffer; // consume trailing "
                return JTOK_STRING;
            case FastPath::NotFullyProcessed:
                // we partially processed, put accepted chars into `tokenVal`
                tokenVal.assign(begin, buffer);
                break; // will take slow path below
            case FastPath::Error:
                // the fast path encountered premature string end or char < 0x20 -- abort early
                return JTOK_ERR;
            }
        }
        // -----
        // Slow path -- scan 1 character at a time and process the chars thru JSONUTF8StringFilter
        // -----
        if constexpr (reserveSize > 0)
            tokenVal.reserve(tokenVal.capacity() + reserveSize);
        JSONUTF8StringFilter writer(tokenVal); // note: this filter object must *not* clear tokenVal in its c'tor

        for (;;) {
            if (static_cast<unsigned char>(*buffer) < 0x20)
                return JTOK_ERR;

            else if (*buffer == '\\') {
                switch (*++buffer) {             // skip backslash, read then skip esc'd char
                case '"':  writer.push_back('"'); ++buffer; break;
                case '\\': writer.push_back('\\'); ++buffer; break;
                case '/':  writer.push_back('/'); ++buffer; break;
                case 'b':  writer.push_back('\b'); ++buffer; break;
                case 'f':  writer.push_back('\f'); ++buffer; break;
                case 'n':  writer.push_back('\n'); ++buffer; break;
                case 'r':  writer.push_back('\r'); ++buffer; break;
                case 't':  writer.push_back('\t'); ++buffer; break;
                case 'u':
                    if (auto optCodepoint = hatoui(++buffer)) { // skip u
                        writer.push_back_u(*optCodepoint);
                        break;
                    }
                    [[fallthrough]];
                default:
                    return JTOK_ERR; // unexpected escape or '\0' after '\' char or codepoint failure
                } // switch

            }

            else if (*buffer == '"') {
                ++buffer;                        // skip "
                break;                           // stop scanning
            }

            else {
                writer.push_back(*buffer++);
            }
        }

        if (!writer.finalize())
            return JTOK_ERR;

        if constexpr (reserveSize > 0)
            tokenVal.shrink_to_fit();
        // -- At this point `tokenVal` contains the entire accepted string from
        // -- inside the enclosing quotes "", unescaped and UTF-8-processed.
        return JTOK_STRING;
        }

    default:
        return JTOK_ERR;
    } // switch
}

} // end anonymous namespace

namespace univalue_internal {
std::optional<std::string> validateAndStripNumStr(const char* s)
{
    std::optional<std::string> ret;
    // string must contain a number and no junk at the end
    if (std::string tokenVal, dummy; getJsonToken(tokenVal, s) == JTOK_NUMBER && getJsonToken(dummy, s) == JTOK_NONE) {
        ret.emplace(std::move(tokenVal));
    }
    return ret;
}
namespace {
bool ParsePrechecks(const std::string& str) noexcept
{
    if (str.empty()) // No empty string allowed
        return false;
    if (json_isspace(str[0]) || json_isspace(str[str.size()-1])) // No padding allowed
        return false;
    if (str.size() != std::strlen(str.c_str())) // No embedded NUL characters allowed
        return false;
    return true;
}
} // namespace
bool ParseInt32(const std::string& str, int32_t *out) noexcept
{
    if (!ParsePrechecks(str))
        return false;
    char *endp = NULL;
    errno = 0; // strtol will not set errno if valid
    long int n = std::strtol(str.c_str(), &endp, 10);
    if (out) *out = (int32_t)n;
    // Note that strtol returns a *long int*, so even if strtol doesn't report an over/underflow
    // we still have to check that the returned value is within the range of an *int32_t*. On 64-bit
    // platforms the size of these types may be different.
    return endp && *endp == 0 && !errno &&
        n >= std::numeric_limits<int32_t>::min() &&
        n <= std::numeric_limits<int32_t>::max();
}
bool ParseInt64(const std::string& str, int64_t *out) noexcept
{
    if (!ParsePrechecks(str))
        return false;
    char *endp = NULL;
    errno = 0; // strtoll will not set errno if valid
    long long int n = std::strtoll(str.c_str(), &endp, 10);
    if(out) *out = (int64_t)n;
    // Note that strtoll returns a *long long int*, so even if strtol doesn't report a over/underflow
    // we still have to check that the returned value is within the range of an *int64_t*.
    return endp && *endp == 0 && !errno &&
        n >= std::numeric_limits<int64_t>::min() &&
        n <= std::numeric_limits<int64_t>::max();
}
bool ParseDouble(const std::string& str, double *out)
{
    if (!ParsePrechecks(str))
        return false;
    if (str.size() >= 2 && str[0] == '0' && str[1] == 'x') // No hexadecimal floats allowed
        return false;
    std::istringstream text(str);
    text.imbue(std::locale::classic());
    double result;
    text >> result;
    if(out) *out = result;
    return text.eof() && !text.fail();
}
} // namespace univalue_interanal

const char* UniValue::read(const char* buffer, const char** errpos)
{
    // wrapped nested function (we catch its return and possibly set errpos)
    const char * const ret = [this, &buffer]() -> const char * {
        setNull(); // clear this
        enum expect_bits {
            EXP_OBJ_NAME = (1U << 0),
            EXP_COLON = (1U << 1),
            EXP_ARR_VALUE = (1U << 2),
            EXP_VALUE = (1U << 3),
            EXP_NOT_VALUE = (1U << 4),
        };
#define expect(bit) (expectMask & (EXP_##bit))
#define setExpect(bit) (expectMask |= EXP_##bit)
#define clearExpect(bit) (expectMask &= ~EXP_##bit)

        uint32_t expectMask = 0;
        std::vector<UniValue*> stack;

        std::string tokenVal;
        jtokentype tok = JTOK_NONE;
        jtokentype last_tok = JTOK_NONE;
        do {
            last_tok = tok;

            tok = getJsonToken(tokenVal, buffer);
            if (tok == JTOK_NONE || tok == JTOK_ERR)
                return nullptr;

            bool isValueOpen = jsonTokenIsValue(tok) ||
                tok == JTOK_OBJ_OPEN || tok == JTOK_ARR_OPEN;

            if (expect(VALUE)) {
                if (!isValueOpen)
                    return nullptr;
                clearExpect(VALUE);

            } else if (expect(ARR_VALUE)) {
                bool isArrValue = isValueOpen || (tok == JTOK_ARR_CLOSE);
                if (!isArrValue)
                    return nullptr;

                clearExpect(ARR_VALUE);

            } else if (expect(OBJ_NAME)) {
                bool isObjName = (tok == JTOK_OBJ_CLOSE || tok == JTOK_STRING);
                if (!isObjName)
                    return nullptr;

            } else if (expect(COLON)) {
                if (tok != JTOK_COLON)
                    return nullptr;
                clearExpect(COLON);

            } else if (!expect(COLON) && (tok == JTOK_COLON)) {
                return nullptr;
            }

            if (expect(NOT_VALUE)) {
                if (isValueOpen)
                    return nullptr;
                clearExpect(NOT_VALUE);
            }

            switch (tok) {

            case JTOK_OBJ_OPEN:
            case JTOK_ARR_OPEN: {
                VType utyp = (tok == JTOK_OBJ_OPEN ? VOBJ : VARR);
                if (!stack.size()) {
                    if (utyp == VOBJ)
                        setObject();
                    else
                        setArray();
                    stack.push_back(this);
                } else {
                    UniValue *top = stack.back();
                    if (top->typ == VOBJ) {
                        auto& value = top->u.entries.rbegin()->second;
                        if (utyp == VOBJ)
                            value.setObject();
                        else
                            value.setArray();
                        stack.push_back(&value);
                    } else {
                        top->u.values.emplace_back(utyp);
                        stack.push_back(&*top->u.values.rbegin());
                    }
                }

                if (stack.size() > MAX_JSON_DEPTH)
                    return nullptr;

                if (utyp == VOBJ)
                    setExpect(OBJ_NAME);
                else
                    setExpect(ARR_VALUE);
                break;
                }

            case JTOK_OBJ_CLOSE:
            case JTOK_ARR_CLOSE: {
                if (!stack.size() || (last_tok == JTOK_COMMA))
                    return nullptr;

                VType utyp = (tok == JTOK_OBJ_CLOSE ? VOBJ : VARR);
                UniValue *top = stack.back();
                if (utyp != top->getType())
                    return nullptr;

                stack.pop_back();
                clearExpect(OBJ_NAME);
                setExpect(NOT_VALUE);
                break;
                }

            case JTOK_COLON: {
                if (!stack.size())
                    return nullptr;

                UniValue *top = stack.back();
                if (top->getType() != VOBJ)
                    return nullptr;

                setExpect(VALUE);
                break;
                }

            case JTOK_COMMA: {
                if (!stack.size() || (last_tok == JTOK_COMMA) || (last_tok == JTOK_ARR_OPEN))
                    return nullptr;

                UniValue *top = stack.back();
                if (top->getType() == VOBJ)
                    setExpect(OBJ_NAME);
                else
                    setExpect(ARR_VALUE);
                break;
                }

            case JTOK_KW_NULL:
            case JTOK_KW_TRUE:
            case JTOK_KW_FALSE: {
                UniValue tmpVal;
                switch (tok) {
                case JTOK_KW_NULL:
                    // do nothing more
                    break;
                case JTOK_KW_TRUE:
                    tmpVal = true;
                    break;
                case JTOK_KW_FALSE:
                    tmpVal = false;
                    break;
                default: /* impossible */ break;
                }

                if (!stack.size()) {
                    *this = std::move(tmpVal);
                    break;
                }

                UniValue *top = stack.back();
                if (top->typ == VOBJ) {
                    top->u.entries.rbegin()->second = std::move(tmpVal);
                } else {
                    top->u.values.emplace_back(std::move(tmpVal));
                }

                setExpect(NOT_VALUE);
                break;
                }

            case JTOK_NUMBER: {
                UniValue tmpVal(VNUM, std::move(tokenVal));
                if (!stack.size()) {
                    *this = std::move(tmpVal);
                    break;
                }

                UniValue *top = stack.back();
                if (top->typ == VOBJ) {
                    top->u.entries.rbegin()->second = std::move(tmpVal);
                } else {
                    top->u.values.emplace_back(std::move(tmpVal));
                }

                setExpect(NOT_VALUE);
                break;
                }

            case JTOK_STRING: {
                if (expect(OBJ_NAME)) {
                    UniValue *top = stack.back();
                    top->u.entries.emplace_back(std::piecewise_construct,
                                                std::forward_as_tuple(std::move(tokenVal)),
                                                std::forward_as_tuple());
                    clearExpect(OBJ_NAME);
                    setExpect(COLON);
                } else {
                    UniValue tmpVal(VSTR, std::move(tokenVal));
                    if (!stack.size()) {
                        *this = std::move(tmpVal);
                        break;
                    }
                    UniValue *top = stack.back();
                    if (top->typ == VOBJ) {
                        top->u.entries.rbegin()->second = std::move(tmpVal);
                    } else {
                        top->u.values.emplace_back(std::move(tmpVal));
                    }
                }

                setExpect(NOT_VALUE);
                break;
                }

            default:
                return nullptr;
            }
        } while (!stack.empty());

        /* Check that nothing follows the initial construct (parsed above).  */
        tok = getJsonToken(tokenVal, buffer);
        if (tok != JTOK_NONE) {
            return nullptr;
        }

        return buffer;
#undef expect
#undef setExpect
#undef clearExpect
    }();

    // if caller specfied errpos pointer, set it
    if (errpos) {
        *errpos = ret == nullptr ? buffer : nullptr;
    }

    return ret;
}

bool UniValue::read(const std::string& raw, std::string::size_type *errpos)
{
    // JSON containing unescaped NUL characters is invalid.
    // std::string is NUL-terminated but may also contain NULs within its size.
    // So read until the first NUL character, and then verify that this is indeed the terminating NUL.
    const char* errptr;
    const char* const res = read(raw.data(), &errptr);
    if (errpos) *errpos = errptr ? errptr - raw.data() : std::string::npos;
    if (res == raw.data() + raw.size()) {
        // parsing consumed entire string (no embedded NULs), success
        return true;
    } else if (!errptr && errpos && res) {
        // Embedded NUL, but no parse error, however parsing is incomplete because it did not consume the entire string.
        // Update errpos accordingly to point to parse end.
        *errpos = res - raw.data();
    }
    return false;
}
