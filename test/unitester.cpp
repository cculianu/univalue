// Copyright 2014 BitPay Inc.
// Copyright (c) 2020-2021 The Bitcoin developers
// Copyright (c) 2021 Calin A. Culianu <calin.culianu@gmail.com>
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.
#ifdef NDEBUG
#undef NDEBUG /* ensure assert() does something */
#endif

#include "univalue.h"

#include <array>
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>

#ifndef JSON_TEST_SRC
#error JSON_TEST_SRC must point to test source directory
#endif

namespace {

const std::string srcdir(JSON_TEST_SRC);
static bool test_failed = false;

#define r_assert(expr) { if (!(expr)) { test_failed = true; ret = false; fprintf(stderr, "%s read failed\n", filename.c_str()); } }
#define w_assert(expr) { if (!(expr)) { test_failed = true; ret = false; fprintf(stderr, "%s write failed\n", filename.c_str()); } }
#define f_assert(expr) { if (!(expr)) { test_failed = true; ret = false; fprintf(stderr, "%s failed\n", __func__); } }

std::string rtrim(std::string s)
{
    s.erase(s.find_last_not_of(" \n\r\t")+1);
    return s;
}

bool runtest(const std::string& filename, const std::string& jdata)
{
        bool ret = true;
        std::string prefix = filename.substr(0, 4);

        bool wantPrettyRoundTrip = prefix == "pret";
        bool wantRoundTrip = wantPrettyRoundTrip || prefix == "roun";
        bool wantPass = wantRoundTrip || prefix == "pass";
        bool wantFail = prefix == "fail";

        assert(wantPass || wantFail);

        UniValue val;
        const bool testResult = val.read(jdata);

        r_assert(testResult == wantPass);
        if (wantRoundTrip) {
            std::string odata = UniValue::stringify(val, wantPrettyRoundTrip ? 4 : 0);
            w_assert(odata == rtrim(jdata));
        }
        return ret;
}

bool runtest_file(const std::string &basename)
{
        std::string filename = srcdir + "/" + basename;
        FILE *f = std::fopen(filename.c_str(), "r");
        assert(f != nullptr);

        std::string jdata;

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

        return runtest(basename, jdata);
}

const char *filenames[] = {
        "fail1.json",
        "fail2.json",
        "fail3.json",
        "fail4.json",   // extra comma
        "fail5.json",
        "fail6.json",
        "fail7.json",
        "fail8.json",
        "fail9.json",   // extra comma
        "fail10.json",
        "fail11.json",
        "fail12.json",
        "fail13.json",
        "fail14.json",
        "fail15.json",
        "fail16.json",
        "fail17.json",
        "fail19.json",
        "fail20.json",
        "fail21.json",
        "fail22.json",
        "fail23.json",
        "fail24.json",
        "fail25.json",
        "fail26.json",
        "fail27.json",
        "fail28.json",
        "fail29.json",
        "fail30.json",
        "fail31.json",
        "fail32.json",
        "fail33.json",
        "fail34.json",
        "fail35.json",
        "fail36.json",
        "fail37.json",
        "fail38.json",  // invalid unicode: only first half of surrogate pair
        "fail39.json",  // invalid unicode: only second half of surrogate pair
        "fail40.json",  // invalid unicode: broken UTF-8
        "fail41.json",  // invalid unicode: unfinished UTF-8
        "fail42.json",  // valid json with garbage following a nul byte
        "fail44.json",  // unterminated string
        "fail45.json",  // nested beyond max depth
        "fail46.json",  // nested beyond max depth, with whitespace
        "fail47.json",  // buffer consisting of only a hyphen (chars: {'-', '\0'})
        "fail48.json",  // -00 is not a valid JSON number
        "fail49.json",  // 0123 is not a valid JSON number
        "fail50.json",  // -1. is not a valid JSON number (no ending in decimals)
        "fail51.json",  // 1.3e+ is not valid (must have a number after the "e+" part)
        "fail52.json",  // reject -[non-digit]
        "pass1.json",
        "pass2.json",
        "pass3.json",
        "round1.json",  // round-trip test
        "round2.json",  // unicode
        "round3.json",  // bare string
        "round4.json",  // bare number
        "round5.json",  // bare true
        "round6.json",  // bare false
        "round7.json",  // bare null
        "round8.json",  // nested at max depth
        "round9.json",  // accept bare buffer containing only "-0"
        "pretty1.json",
        "pretty2.json",
};

// Test \u handling
bool unescape_unicode_test()
{
    UniValue val;
    bool testResult, ret = true;
    // Escaped ASCII (quote)
    testResult = val.read("[\"\\u0022\"]");
    f_assert(testResult);
    f_assert(val[0].get_str() == "\"");
    // Escaped Basic Plane character, two-byte UTF-8
    testResult = val.read("[\"\\u0191\"]");
    f_assert(testResult);
    f_assert(val[0].get_str() == "\xc6\x91");
    // Escaped Basic Plane character, three-byte UTF-8
    testResult = val.read("[\"\\u2191\"]");
    f_assert(testResult);
    f_assert(val[0].get_str() == "\xe2\x86\x91");
    // Escaped Supplementary Plane character U+1d161
    testResult = val.read("[\"\\ud834\\udd61\"]");
    f_assert(testResult);
    f_assert(val[0].get_str() == "\xf0\x9d\x85\xa1");
    return ret;
}

} // namespace

int main()
{
    for (std::size_t fidx = 0; fidx < std::size(filenames); ++fidx) {
        const auto & fname = filenames[fidx];
        std::cerr << "Running test on \"" << fname << "\" ... " << std::flush;
        if (runtest_file(fname)) std::cerr << "OK" << std::endl;
        else std::cerr << "Failed!" << std::endl;
    }

    std::cerr << "Running \"unescape_unicode_test\" ... " << std::flush;
    if (unescape_unicode_test()) std::cerr << "OK" << std::endl;
    else std::cerr << "Failed!" << std::endl;

    return test_failed ? 1 : 0;
}

