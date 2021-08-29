// Copyright (c) 2021 Calin A. Culianu <calin.culianu@gmail.com>
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.
#ifdef NDEBUG
#undef NDEBUG /* ensure assert() does something */
#endif

#include "univalue.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cinttypes>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>

#ifdef _WIN32
// The below are only used by GetPerfTimeNanos() in the Windows case
#include <profileapi.h>
#endif

#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
#define HAVE_NLOHMANN
#endif

namespace {

int64_t GetPerfTimeNanos() {
#ifdef _WIN32
    // Windows lacks a decent high resolution clock source on some C++ implementations (such as MinGW). So we
    // query the OS's QPC mechanism, which, on Windows 7+ is very fast to query and guaranteed to be accurate,
    // with sub-microsecond precision. It is also monotonic ("steady").
    static const auto queryPerfCtr = []() -> int64_t {
        // This is is guaranteed to be initialized once atomically the first time through this function.
        static const int64_t freq = []{
            // Read the performance counter frequency in counts per second.
            LARGE_INTEGER val;
            QueryPerformanceFrequency(&val);
            return int64_t(val.QuadPart);
        }();
        LARGE_INTEGER ct;
        QueryPerformanceCounter(&ct);   // reads the performance counter (in ticks)
        // return ticks converted to nanoseconds
        return (int64_t(ct.QuadPart) * int64_t(1'000'000'000)) / std::max(freq, int64_t(1));
    };
    // Save the timestamp of the first time through here to start with low values after app init.
    static const int64_t t0 = queryPerfCtr();
    return queryPerfCtr() - t0;
#else
    using Clock = std::conditional_t<std::chrono::high_resolution_clock::is_steady,
                                     std::chrono::high_resolution_clock,
                                     std::chrono::steady_clock>;
    static const auto t0 = Clock::now();
    return std::chrono::duration<int64_t, std::nano>(Clock::now() - t0).count();
#endif
}

/// Tic, or "time-code". A convenience class that can be used to profile sections of code.
///
/// Takes a timestamp (via GetPerfTimeNanos) when it is constructed. Provides various utility
/// methods to read back the elapsed time since construction, in various units.
class Tic {
    static int64_t now() { return GetPerfTimeNanos(); }
    int64_t saved = now();
    template <typename T>
    using T_if_is_arithmetic = std::enable_if_t<std::is_arithmetic_v<T>, T>; // SFINAE evaluates to T if T is an arithmetic type
    template <typename T>
    T_if_is_arithmetic<T> elapsed(T factor) const {
        if (factor <= 0) return std::numeric_limits<T>::max();
        if (saved >= 0) {
            // non-frozen timestamp; `saved` is the timestamp at construction
            return T((now() - saved)) / factor;
        } else {
            // frozen timestamp; `saved` is the negative of the elapsed time when fin() was called
            return T(std::abs(saved)) / factor;
        }
    }
public:
    static std::string format(double val, int precision);
    static std::string format(int64_t nsec);

    /// Returns the number of seconds elapsed since construction (note the default return type here is double)
    template <typename T = double>
    T_if_is_arithmetic<T>
    /* T */ secs() const { return elapsed(T(1e9)); }

    /// Returns the number of seconds elapsed since construction formatted as a floating point string (for logging)
    std::string secsStr(int precision = 3) const { return format(secs(), precision); }

    /// " milliseconds (note the default return type here is int64_t)
    template <typename T = int64_t>
    T_if_is_arithmetic<T>
    /* T */ msec() const { return elapsed(T(1e6)); }

    /// Returns the number of milliseconds formatted as a floating point string, useful for logging
    std::string msecStr(int precision = 3) const { return format(msec<double>(), precision); }

    /// " microseconds (note the default return type here is int64_t)
    template <typename T = int64_t>
    T_if_is_arithmetic<T>
    /* T */ usec() const { return elapsed(T(1e3)); }

    /// Returns the number of microseconds formatted as a floating point string, useful for logging
    std::string usecStr(int precision = 3) const { return format(usec<double>(), precision); }

    /// " nanoseconds
    int64_t nsec() const { return elapsed(int64_t(1)); }

    /// Returns the number of nanoseconds formatted as an integer string, useful for logging
    std::string nsecStr() const { return format(nsec()); }

    /// Save the current time. After calling this, secs(), msec(), usec(), and nsec() above will "freeze"
    /// and they will forever always return the time from construction until fin() was called.
    ///
    /// Subsequent calls to fin() are no-ops.  Once fin()'d a Tic cannot be set to continue counting time.
    /// To restart the timer, assign it a default constructed value and it will begin counting again from
    /// that point forward e.g.:  mytic = Tic()
    void fin() { saved = -nsec(); }
};

/* static */
std::string Tic::format(double val, int precision) {
    char buf[64];
    if (precision < 0) precision = 0;
    if (precision > 32) precision = 32;
    std::snprintf(buf, sizeof(buf), "%1.*f", precision, val);
    return buf;
}

/* static */
std::string Tic::format(int64_t nsec) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%" PRId64, nsec);
    return buf;
}

bool runbench_univalue(const size_t N, const std::string &jdata)
{
    assert(N > 0);
    std::cout << "Parsing and re-serializeing " << N << " times ...\n";
    std::vector<Tic> parseTimes, serializeTimes;
    parseTimes.reserve(N); serializeTimes.reserve(N);
    std::vector<std::string> strings;
    strings.reserve(2);
    Tic t0;
    for (size_t i = 0; i < N; ++i) {
        UniValue uv;

        parseTimes.emplace_back(); // start timer
        if ( ! uv.read(jdata)) {
            std::cout << "Failed to parse on iteration " << i << "!\n";
            return false;
        }
        parseTimes.back().fin(); // freeze timer

        serializeTimes.emplace_back(); // start timer
        strings.push_back( UniValue::stringify(uv, 4) );
        serializeTimes.back().fin(); // freeze timer

        // check strings -- this is to ensure uv.stringify() is not a no-op above
        assert(strings.size() < 2 || strings[strings.size()-1] == strings[strings.size() - 1]);
        strings.resize(1); // throw away old strings
    }
    t0.fin();
    assert(!parseTimes.empty() && !serializeTimes.empty());
    const auto Compare = [](const Tic &a, const Tic &b){ return a.nsec() < b.nsec(); };
    std::sort(parseTimes.begin(), parseTimes.end(), Compare);
    std::sort(serializeTimes.begin(), serializeTimes.end(), Compare);
    const auto &parseMedian = parseTimes[N / 2];
    const auto &serializeMedian = serializeTimes[N / 2];
    int64_t parseAvg{}, serializeAvg{};
    for (const auto &tic : parseTimes) parseAvg += tic.usec();
    for (const auto &tic : serializeTimes) serializeAvg += tic.usec();
    parseAvg /= parseTimes.size();
    serializeAvg /= serializeTimes.size();
    std::cout << "Elapsed (msec) - " << t0.msecStr() << "\n"
              << "Parse (msec) - "
              << "median: " << parseMedian.msecStr()
              << ", avg: " << Tic::format(parseAvg/1e3, 3)
              << ", best: " << parseTimes.front().msecStr()
              << ", worst: " << parseTimes.back().msecStr() << "\n"
              << "Serialize (msec) - "
              << "median: " << serializeMedian.msecStr()
              << ", avg: " << Tic::format(serializeAvg/1e3, 3)
              << ", best: " << serializeTimes.front().msecStr()
              << ", worst: " << serializeTimes.back().msecStr() << "\n";

    return true;
}

#ifdef HAVE_NLOHMANN
void runbench_nlohmann(const size_t N, const std::string &jdata)
{
    assert(N > 0);
    std::cout << "Parsing and re-serializeing " << N << " times ...\n";
    std::vector<Tic> parseTimes, serializeTimes;
    parseTimes.reserve(N); serializeTimes.reserve(N);
    std::vector<std::string> strings;
    strings.reserve(2);
    Tic t0;
    for (size_t i = 0; i < N; ++i) {
        parseTimes.emplace_back(); // start timer
        auto j = nlohmann::json::parse(jdata); // this throws on parse error
        parseTimes.back().fin(); // freeze timer

        serializeTimes.emplace_back(); // start timer
        strings.push_back( j.dump(4) );
        serializeTimes.back().fin(); // freeze timer

        // check strings -- this is to ensure uv.stringify() is not a no-op above
        assert(strings.size() < 2 || strings[strings.size()-1] == strings[strings.size() - 1]);
        strings.resize(1); // throw away old strings
    }
    t0.fin();
    assert(!parseTimes.empty() && !serializeTimes.empty());
    const auto Compare = [](const Tic &a, const Tic &b){ return a.nsec() < b.nsec(); };
    std::sort(parseTimes.begin(), parseTimes.end(), Compare);
    std::sort(serializeTimes.begin(), serializeTimes.end(), Compare);
    const auto &parseMedian = parseTimes[N / 2];
    const auto &serializeMedian = serializeTimes[N / 2];
    int64_t parseAvg{}, serializeAvg{};
    for (const auto &tic : parseTimes) parseAvg += tic.usec();
    for (const auto &tic : serializeTimes) serializeAvg += tic.usec();
    parseAvg /= parseTimes.size();
    serializeAvg /= serializeTimes.size();
    std::cout << "Elapsed (msec) - " << t0.msecStr() << "\n"
              << "Parse (msec) - "
              << "median: " << parseMedian.msecStr()
              << ", avg: " << Tic::format(parseAvg/1e3, 3)
              << ", best: " << parseTimes.front().msecStr()
              << ", worst: " << parseTimes.back().msecStr() << "\n"
              << "Serialize (msec) - "
              << "median: " << serializeMedian.msecStr()
              << ", avg: " << Tic::format(serializeAvg/1e3, 3)
              << ", best: " << serializeTimes.front().msecStr()
              << ", worst: " << serializeTimes.back().msecStr() << "\n";
}
#endif

bool runbench_file(const std::string &path)
{
    const std::string filename = [&]{
        if (auto pos = filename.find_last_of("/\\") != filename.npos)
            return filename.substr(pos + 1);
        return filename;
    }();

    std::string jdata;
    {
        const Tic tread0;
        FILE *f = std::fopen(path.c_str(), "r");
        assert(f != nullptr);


        char buf[16384];
        while (!std::feof(f)) {
            const int bread = std::fread(buf, 1, sizeof(buf), f);
            assert(!std::ferror(f));
            assert(bread >= 0);

            std::string_view s(buf, bread);
            jdata += s;
        }

        assert(!std::ferror(f));
        std::fclose(f);
        std::cout << "Read " << jdata.size() << " bytes in " << tread0.msecStr() << " msec\n";
    }

    constexpr size_t N = 10;
    std::cout << "\n--- UniValue lib ---\n";
    if ( ! runbench_univalue(N, jdata))
        return false;
#ifdef HAVE_NLOHMANN
    std::cout << "\n--- nlohmann::json lib ---\n";
    try {
        runbench_nlohmann(N, jdata);
    } catch (const nlohmann::json::exception &e) {
        std::cout << "nlohmann::json::exception caught: " << e.what() << "\n";
        return false;
    }
#endif
    return true;
}

} // namespace

int main(int argc, char *argv[])
{
#ifndef HAVE_NLOHMANN
    std::cerr << "WARNING: nlohmann::json was not linked to this binary!\n"
              << "WARNING: For best results, please install nlohmann::json and rebuild this program.\n\n";
#endif
    int failct = 0;
    for (int i = 1; i < argc; ++i) {
        const auto & fname = argv[i];
        std::cout << std::string(80, '-') << std::endl;
        std::cout << "Running test on \"" << fname << "\" ..." << std::endl;
        if (!runbench_file(fname)) {
            ++failct;
            std::cout << "FAILED \"" << fname << "\"" << std::endl;
        }
        std::cout << "\n";
    }
    return bool(failct);
}

