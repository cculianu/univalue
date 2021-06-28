// Copyright 2014 BitPay Inc.
// Copyright (c) 2020-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "univalue.h"

#include <array>
#include <cstring>

namespace {
const std::array<const char *, 256> escapes = {{
    "\\u0000",
    "\\u0001",
    "\\u0002",
    "\\u0003",
    "\\u0004",
    "\\u0005",
    "\\u0006",
    "\\u0007",
    "\\b",
    "\\t",
    "\\n",
    "\\u000b",
    "\\f",
    "\\r",
    "\\u000e",
    "\\u000f",
    "\\u0010",
    "\\u0011",
    "\\u0012",
    "\\u0013",
    "\\u0014",
    "\\u0015",
    "\\u0016",
    "\\u0017",
    "\\u0018",
    "\\u0019",
    "\\u001a",
    "\\u001b",
    "\\u001c",
    "\\u001d",
    "\\u001e",
    "\\u001f",
    nullptr,
    nullptr,
    "\\\"",
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    "\\\\",
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    "\\u007f",
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
}};
} // end anonymous namespace

/* static */
void UniValue::jsonEscape(Stream & ss, std::string_view inS)
{
    for (const auto ch : inS) {
        const char * const escStr = escapes[uint8_t(ch)];

        if (escStr)
            ss << escStr;
        else
            ss.put(ch);
    }
}

/* static */
inline void UniValue::startNewLine(Stream & ss, const unsigned int prettyIndent, const unsigned int indentLevel)
{
    if (prettyIndent) {
        ss.put('\n');
        ss.put(' ', indentLevel);
    }
}

/* static */
void UniValue::stringify(Stream& ss, const UniValue& value, const unsigned int prettyIndent, const unsigned int indentLevel)
{
    switch (value.typ) {
    case VNULL:
        ss << "null";
        break;
    case VFALSE:
        ss << "false";
        break;
    case VTRUE:
        ss << "true";
        break;
    case VOBJ:
        stringify(ss, value.entries, prettyIndent, indentLevel);
        break;
    case VARR:
        stringify(ss, value.values, prettyIndent, indentLevel);
        break;
    case VNUM:
        ss << value.val;
        break;
    case VSTR:
        stringify(ss, value.val, prettyIndent, indentLevel);
        break;
    }
}

/* static */
void UniValue::stringify(Stream & ss, const UniValue::Object& object, const unsigned int prettyIndent, const unsigned int indentLevel)
{
    ss.put('{');
    if (!object.empty()) {
        const unsigned int internalIndentLevel = indentLevel + prettyIndent;
        for (auto entry = object.begin(), end = object.end();;) {
            startNewLine(ss, prettyIndent, internalIndentLevel);
            ss.put('"');
            jsonEscape(ss, entry->first);
            ss << "\":";
            if (prettyIndent) {
                ss.put(' ');
            }
            stringify(ss, entry->second, prettyIndent, internalIndentLevel);
            if (++entry == end) {
                break;
            }
            ss.put(',');
        }
    }
    startNewLine(ss, prettyIndent, indentLevel);
    ss.put('}');
}

/* static */
void UniValue::stringify(Stream & ss, const UniValue::Array& array, const unsigned int prettyIndent, const unsigned int indentLevel)
{
    ss.put('[');
    if (!array.empty()) {
        const unsigned int internalIndentLevel = indentLevel + prettyIndent;
        for (auto value = array.begin(), end = array.end();;) {
            startNewLine(ss, prettyIndent, internalIndentLevel);
            stringify(ss, *value, prettyIndent, internalIndentLevel);
            if (++value == end) {
                break;
            }
            ss.put(',');
        }
    }
    startNewLine(ss, prettyIndent, indentLevel);
    ss.put(']');
}

/* static */
void UniValue::stringify(Stream& ss, std::string_view string,
                         const unsigned prettyIndent [[maybe_unused]], const unsigned indentLevel [[maybe_unused]])
{
    ss.put('"');
    jsonEscape(ss, string);
    ss.put('"');
}
