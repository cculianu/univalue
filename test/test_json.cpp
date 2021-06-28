// Test program that can be called by the JSON test suite at
// https://github.com/nst/JSONTestSuite.
//
// It reads JSON input from stdin and exits with code 0 if it can be parsed
// successfully. It also pretty prints the parsed JSON value to stdout.
// Additionally, it prints some benchmark stats to stderr.

#include "univalue.h"

#include <chrono>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <string>
#include <type_traits>

int64_t GetTimeMicros()
{
    using Clock = std::conditional_t<std::chrono::high_resolution_clock::is_steady,
                                     std::chrono::high_resolution_clock,
                                     std::chrono::steady_clock>;
    // Save the timestamp of the first time through here to start with low values after app init.
    static const auto t0 = Clock::now();
    return std::chrono::duration<int64_t, std::nano>(Clock::now() - t0).count() / static_cast<int64_t>(1000);
}

int main()
{
    UniValue val;
    std::string str;
    const auto t0 = GetTimeMicros();
    {
        // read string, 256kb at a time -- this is faster than using slow std::istreambuf, etc
        size_t nread;
        do {
            constexpr size_t chunkSize = 262144;
            size_t realSz = str.size();
            str.resize(realSz + chunkSize, '\0');
            nread = std::fread(str.data() + realSz, 1, chunkSize, stdin);
            realSz += nread;
            str.resize(realSz);
        } while (nread > 0 && !std::feof(stdin) && !std::ferror(stdin));
    }
    const auto t1 = GetTimeMicros();
    if (val.read(str)) {
        const auto t2 = GetTimeMicros();
        auto outStr = UniValue::stringify(val, 1 /* prettyIndent */);
        const auto t3 = GetTimeMicros();
        std::cout << outStr << std::endl;
        std::cerr << "size: " << std::fixed << std::setprecision(3) << (outStr.size()/1e6) << " MB"
                  << ", read: " << ((t1-t0)/1e3) << " msec"
                  << ", parse: " << ((t2-t1)/1e3) << " msec"
                  << ", stringify: " << ((t3-t2)/1e3) << " msec"
                  << std::endl;
        return 0;
    } else {
        std::cerr << "JSON Parse Error." << std::endl;
        return 1;
    }
}
