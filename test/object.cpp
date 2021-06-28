// Copyright (c) 2014 BitPay Inc.
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Copyright (c) 2020-2021 The Bitcoin developers
// Copyright (c) 2021 Calin A. Culianu <calin.culianu@gmail.com>
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.
#ifdef NDEBUG
#undef NDEBUG /* ensure assert() does something */
#endif

#include "univalue.h"

#include <cassert>
#include <clocale>
#include <cstdint>
#include <iostream>
#include <limits>
#include <locale>
#include <stdexcept>
#include <string>
#include <vector>

#define BOOST_FIXTURE_TEST_SUITE(a, b)
#define BOOST_AUTO_TEST_CASE(funcName) void funcName()
#define BOOST_AUTO_TEST_SUITE_END()
#define BOOST_CHECK(expr) do { \
    std::cerr << "Checking: " << (#expr) << " ... " << std::flush; \
    assert(expr); \
    std::cerr << "OK" << std::endl; \
} while(0)
#define BOOST_CHECK_EQUAL(v1, v2) do { \
    std::cerr << "Checking: " << (#v1) << " == " << (#v2) << " ... " << std::flush; \
    assert((v1) == (v2)); \
    std::cerr << "OK" << std::endl; \
} while(0)
#define BOOST_CHECK_THROW(stmt, excMatch) do { \
    try { \
        std::cerr << "Checking: " << (#stmt) << " throws " << (#excMatch) << " ... " << std::flush; \
        (stmt); \
        assert(0 && "No exception caught"); \
    } catch (excMatch & e) { \
        std::cerr << "OK" << std::endl; \
    } catch (...) { \
        assert(0 && "Wrong exception caught"); \
    } \
} while(0)
#define BOOST_CHECK_NO_THROW(stmt) do { \
    try { \
        std::cerr << "Checking: " << (#stmt) << " does not throw ... " << std::flush; \
        (stmt); \
        std::cerr << "OK" << std::endl; \
    } catch (...) { \
        assert(0); \
	} \
} while(0)

BOOST_FIXTURE_TEST_SUITE(univalue_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(univalue_constructor)
{
    UniValue v1;
    BOOST_CHECK(v1.isNull());

    UniValue v2(UniValue::VSTR);
    BOOST_CHECK(v2.isStr());

    UniValue v3(UniValue::VSTR, "foo");
    BOOST_CHECK(v3.isStr());
    BOOST_CHECK_EQUAL(v3.getValStr(), "foo");

    UniValue numTest;
    numTest.setNumStr("82");
    BOOST_CHECK(numTest.isNum());
    BOOST_CHECK_EQUAL(numTest.getValStr(), "82");

    uint64_t vu64 = 82;
    UniValue v4(vu64);
    BOOST_CHECK(v4.isNum());
    BOOST_CHECK_EQUAL(v4.getValStr(), "82");

    int64_t vi64 = -82;
    UniValue v5(vi64);
    BOOST_CHECK(v5.isNum());
    BOOST_CHECK_EQUAL(v5.getValStr(), "-82");

    int vi = -688;
    UniValue v6(vi);
    BOOST_CHECK(v6.isNum());
    BOOST_CHECK_EQUAL(v6.getValStr(), "-688");

    double vd = -7.21;
    UniValue v7(vd);
    BOOST_CHECK(v7.isNum());
    BOOST_CHECK_EQUAL(v7.getValStr(), "-7.21");

    std::string vs("yawn");
    UniValue v8(vs);
    BOOST_CHECK(v8.isStr());
    BOOST_CHECK_EQUAL(v8.getValStr(), "yawn");

    const char *vcs = "zappa";
    UniValue v9(vcs);
    BOOST_CHECK(v9.isStr());
    BOOST_CHECK_EQUAL(v9.getValStr(), "zappa");
}

BOOST_AUTO_TEST_CASE(univalue_typecheck)
{
    UniValue v1;
    v1.setNumStr("1");
    BOOST_CHECK(v1.isNum());
    BOOST_CHECK_THROW(v1.get_bool(), std::runtime_error);

    UniValue v2;
    v2 = true;
    BOOST_CHECK_EQUAL(v2.get_bool(), true);
    BOOST_CHECK_THROW(v2.get_int(), std::runtime_error);

    UniValue v3;
    v3.setNumStr("32482348723847471234");
    BOOST_CHECK_THROW(v3.get_int64(), std::runtime_error);
    v3.setNumStr("1000");
    BOOST_CHECK_EQUAL(v3.get_int64(), 1000);

    UniValue v4;
    v4.setNumStr("2147483648");
    BOOST_CHECK_EQUAL(v4.get_int64(), 2147483648);
    BOOST_CHECK_THROW(v4.get_int(), std::runtime_error);
    v4.setNumStr("1000");
    BOOST_CHECK_EQUAL(v4.get_int(), 1000);
    BOOST_CHECK_THROW(v4.get_str(), std::runtime_error);
    BOOST_CHECK_EQUAL(v4.get_real(), 1000);
    BOOST_CHECK_THROW(v4.get_array(), std::runtime_error);
    BOOST_CHECK_THROW(v4.get_obj(), std::runtime_error);

    UniValue v5;
    BOOST_CHECK(v5.read("[true, 10]"));
    UniValue::Array& vals = v5.get_array();
    BOOST_CHECK_THROW(vals[0].get_int(), std::runtime_error);
    BOOST_CHECK_EQUAL(vals[0].get_bool(), true);

    BOOST_CHECK_EQUAL(vals[1].get_int(), 10);
    BOOST_CHECK_THROW(vals[1].get_bool(), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(univalue_set)
{
    UniValue v(UniValue::VSTR, "foo");
    v.setNull();
    BOOST_CHECK(v.isNull());
    BOOST_CHECK_EQUAL(v.getValStr(), "");
    BOOST_CHECK_EQUAL(v.size(), 0);
    BOOST_CHECK_EQUAL(v.getType(), UniValue::VNULL);
    BOOST_CHECK(v.empty());

    UniValue::Object& obj = v.setObject();
    BOOST_CHECK_EQUAL(&obj, &v.get_obj());
    BOOST_CHECK(v.isObject());
    BOOST_CHECK_EQUAL(v.size(), 0);
    BOOST_CHECK_EQUAL(v.getType(), UniValue::VOBJ);
    BOOST_CHECK(v.empty());
    UniValue vo(v); // make a copy
    BOOST_CHECK(vo.isObject());
    BOOST_CHECK_EQUAL(vo.size(), 0);
    BOOST_CHECK_EQUAL(vo.getType(), UniValue::VOBJ);
    BOOST_CHECK(vo.empty());
    obj.emplace_back("key", "value"); // change the original
    BOOST_CHECK_EQUAL(v.size(), 1);
    BOOST_CHECK(!v.empty());
    BOOST_CHECK_EQUAL(vo.size(), 0);
    BOOST_CHECK(vo.empty());
    UniValue::Object& obj2 = vo = obj; // assign original to copy
    BOOST_CHECK_EQUAL(&obj2, &vo.get_obj());
    BOOST_CHECK_EQUAL(vo.size(), 1);
    BOOST_CHECK(!vo.empty());

    UniValue::Array& arr = v.setArray();
    BOOST_CHECK_EQUAL(&arr, &v.get_array());
    BOOST_CHECK(v.isArray());
    BOOST_CHECK_EQUAL(v.size(), 0);
    BOOST_CHECK_EQUAL(v.getType(), UniValue::VARR);
    BOOST_CHECK(v.empty());
    UniValue va(v); // make a copy
    BOOST_CHECK(va.isArray());
    BOOST_CHECK_EQUAL(va.size(), 0);
    BOOST_CHECK_EQUAL(va.getType(), UniValue::VARR);
    BOOST_CHECK(va.empty());
    arr.emplace_back("value"); // change the original
    BOOST_CHECK_EQUAL(v.size(), 1);
    BOOST_CHECK(!v.empty());
    BOOST_CHECK_EQUAL(va.size(), 0);
    BOOST_CHECK(va.empty());
    UniValue::Array& arr2 = va = arr; // assign original to copy
    BOOST_CHECK_EQUAL(&arr2, &va.get_array());
    BOOST_CHECK_EQUAL(va.size(), 1);
    BOOST_CHECK(!va.empty());

    std::string& str = v = "zum";
    BOOST_CHECK_EQUAL(&str, &v.get_str());
    BOOST_CHECK(v.isStr());
    BOOST_CHECK_EQUAL(v.getValStr(), "zum");
    BOOST_CHECK_EQUAL(v.size(), 0);
    BOOST_CHECK_EQUAL(v.getType(), UniValue::VSTR);
    BOOST_CHECK(v.empty());

    v = "string must change into null when assigning unrepresentable number";
    BOOST_CHECK(v.isStr());
    v = std::numeric_limits<double>::quiet_NaN();
    BOOST_CHECK(v.isNull());

    v = "string must change into null when assigning unrepresentable number";
    BOOST_CHECK(v.isStr());
    v = std::numeric_limits<double>::signaling_NaN();
    BOOST_CHECK(v.isNull());

    v = "string must change into null when assigning unrepresentable number";
    BOOST_CHECK(v.isStr());
    v = std::numeric_limits<double>::infinity();
    BOOST_CHECK(v.isNull());

    v = "string must change into null when assigning unrepresentable number";
    BOOST_CHECK(v.isStr());
    v = -std::numeric_limits<double>::infinity();
    BOOST_CHECK(v.isNull());

    v = -1.01;
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "-1.01");
    BOOST_CHECK_EQUAL(v.size(), 0);
    BOOST_CHECK_EQUAL(v.getType(), UniValue::VNUM);
    BOOST_CHECK(v.empty());

    v.setNull();
    BOOST_CHECK(v.isNull());
    v = -std::numeric_limits<double>::max();
    BOOST_CHECK(v.isNum());

    v = -1.79769313486231570e+308;
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "-1.797693134862316e+308");

    v = -100000000000000000. / 3.;
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "-3.333333333333333e+16");

    v = -10000000000000000. / 3.;
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "-3333333333333334");

    v = -1000000000000000. / 3.;
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "-333333333333333.3");

    v = -10. / 3.;
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "-3.333333333333333");

    v = -1.;
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "-1");

    v = -1. / 3.;
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "-0.3333333333333333");

    v = -1. / 3000.;
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "-0.0003333333333333333");

    v = -1. / 30000.;
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "-3.333333333333333e-05");

    v = -4.94065645841246544e-324;
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "-4.940656458412465e-324");

    v.setNull();
    BOOST_CHECK(v.isNull());
    v = -std::numeric_limits<double>::min();
    BOOST_CHECK(v.isNum());

    v = -1. / std::numeric_limits<double>::infinity();
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "-0");

    v = 1. / std::numeric_limits<double>::infinity();
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "0");

    v.setNull();
    BOOST_CHECK(v.isNull());
    v = std::numeric_limits<double>::min();
    BOOST_CHECK(v.isNum());

    v = 4.94065645841246544e-324;
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "4.940656458412465e-324");

    v = 1. / 30000.;
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "3.333333333333333e-05");

    v = 1. / 3000.;
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "0.0003333333333333333");

    v = 1. / 3.;
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "0.3333333333333333");

    v = 1.;
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "1");

    v = 10. / 3.;
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "3.333333333333333");

    v = 1000000000000000. / 3.;
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "333333333333333.3");

    v = 10000000000000000. / 3.;
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "3333333333333334");

    v = 100000000000000000. / 3.;
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "3.333333333333333e+16");

    v = 1.79769313486231570e+308;
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "1.797693134862316e+308");

    v.setNull();
    BOOST_CHECK(v.isNull());
    v = std::numeric_limits<double>::max();
    BOOST_CHECK(v.isNum());

    v = 1023;
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "1023");

    v = 0;
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "0");

    v = std::numeric_limits<int>::min();
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), std::to_string(std::numeric_limits<int>::min()));

    v = std::numeric_limits<int>::max();
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), std::to_string(std::numeric_limits<int>::max()));

    v = int64_t(-1023);
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "-1023");

    v = int64_t(0);
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "0");

    v = std::numeric_limits<int64_t>::min();
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "-9223372036854775808");

    v = std::numeric_limits<int64_t>::max();
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "9223372036854775807");

    v = uint64_t(1023);
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "1023");

    v = uint64_t(0);
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "0");

    v = std::numeric_limits<uint64_t>::max();
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "18446744073709551615");

    v.setNull();
    v.setNumStr("-688");
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "-688");

    v.setNull();
    /* ensure whitepsace was stripped */
    v.setNumStr(" -689 ");
    BOOST_CHECK(v.isNum());
    BOOST_CHECK_EQUAL(v.getValStr(), "-689");

    v.setNull();
    v.setNumStr(" -690 whitespace then junk");
    BOOST_CHECK(v.isNull());

    v.setNull();
    v.setNumStr("-688_has_junk_at_end");
    BOOST_CHECK(v.isNull());

    v = false;
    BOOST_CHECK_EQUAL(v.isBool(), true);
    BOOST_CHECK_EQUAL(v.isTrue(), false);
    BOOST_CHECK_EQUAL(v.isFalse(), true);
    BOOST_CHECK_EQUAL(v.getBool(), false);

    v = true;
    BOOST_CHECK_EQUAL(v.isBool(), true);
    BOOST_CHECK_EQUAL(v.isTrue(), true);
    BOOST_CHECK_EQUAL(v.isFalse(), false);
    BOOST_CHECK_EQUAL(v.getBool(), true);

    v.setNumStr("zombocom");
    BOOST_CHECK_EQUAL(v.isBool(), true);
    BOOST_CHECK_EQUAL(v.isTrue(), true);
    BOOST_CHECK_EQUAL(v.isFalse(), false);
    BOOST_CHECK_EQUAL(v.getBool(), true);

    v.setNull();
    BOOST_CHECK(v.isNull());
}

BOOST_AUTO_TEST_CASE(univalue_array)
{
    UniValue arr(UniValue::VARR);

    UniValue v((int64_t)1023LL);
    arr.get_array().push_back(v);

    arr.get_array().emplace_back("zippy");

    const char *s = "pippy";
    arr.get_array().push_back(s);

    UniValue::Array vec;
    v = "boing";
    vec.push_back(v);

    v = "going";
    vec.push_back(std::move(v));

    for (UniValue& thing : vec) {
        // emplace with copy constructor of UniValue
        arr.get_array().emplace_back(thing);
    }

    arr.get_array().emplace_back((uint64_t) 400ULL);
    arr.get_array().push_back((int64_t) -400LL);
    arr.get_array().emplace_back((int) -401);
    arr.get_array().push_back(-40.1);

    BOOST_CHECK_EQUAL(arr.empty(), false);
    BOOST_CHECK_EQUAL(arr.size(), 9);

    BOOST_CHECK_EQUAL(arr[0].getValStr(), "1023");
    BOOST_CHECK_EQUAL(arr[1].getValStr(), "zippy");
    BOOST_CHECK_EQUAL(arr[2].getValStr(), "pippy");
    BOOST_CHECK_EQUAL(arr[3].getValStr(), "boing");
    BOOST_CHECK_EQUAL(arr[4].getValStr(), "going");
    BOOST_CHECK_EQUAL(arr[5].getValStr(), "400");
    BOOST_CHECK_EQUAL(arr[6].getValStr(), "-400");
    BOOST_CHECK_EQUAL(arr[7].getValStr(), "-401");
    BOOST_CHECK_EQUAL(arr[8].getValStr(), "-40.1");

    BOOST_CHECK_EQUAL(arr[9].getValStr(), "");
    BOOST_CHECK_EQUAL(arr["nyuknyuknyuk"].getValStr(), "");

    BOOST_CHECK_EQUAL(&arr[9], &UniValue::NullUniValue);
    BOOST_CHECK_EQUAL(&arr["nyuknyuknyuk"], &UniValue::NullUniValue);

    BOOST_CHECK_EQUAL(arr.locate("nyuknyuknyuk"), nullptr);

    for (int i = 0; i < 9; ++i)
        BOOST_CHECK_EQUAL(&arr.at(i), &arr[i]);

    BOOST_CHECK_THROW(arr.at(9), std::out_of_range);
    BOOST_CHECK_THROW(arr.at("nyuknyuknyuk"), std::domain_error);

    // erase zippy and pippy
    auto after = arr.get_array().erase(arr.get_array().begin() + 1, arr.get_array().begin() + 3);

    BOOST_CHECK_EQUAL(arr.get_array().empty(), false);
    BOOST_CHECK_EQUAL(arr.get_array().size(), 7);
    BOOST_CHECK_EQUAL(arr.at(0).getValStr(), "1023");
    BOOST_CHECK_EQUAL(arr.at(1).getValStr(), "boing");
    BOOST_CHECK_EQUAL(*after, "boing");

    arr.setNull();
    BOOST_CHECK(arr.empty());
    BOOST_CHECK_EQUAL(arr.size(), 0);
    BOOST_CHECK_EQUAL(arr.getType(), UniValue::VNULL);
    BOOST_CHECK_EQUAL(&arr[0], &UniValue::NullUniValue);
    BOOST_CHECK_EQUAL(&arr["nyuknyuknyuk"], &UniValue::NullUniValue);
    BOOST_CHECK_EQUAL(arr.locate("nyuknyuknyuk"), nullptr);
    BOOST_CHECK_THROW(arr.at(0), std::domain_error);
    BOOST_CHECK_THROW(arr.at("nyuknyuknyuk"), std::domain_error);
}

BOOST_AUTO_TEST_CASE(univalue_object)
{
    UniValue obj(UniValue::VOBJ);
    std::string strKey, strVal;
    UniValue v;

    BOOST_CHECK(obj.empty());
    BOOST_CHECK_EQUAL(obj.size(), 0);

    strKey = "age";
    v = 100;
    obj.get_obj().emplace_back(strKey, v);

    strKey = "first";
    strVal = "John";
    obj.get_obj().emplace_back(strKey, strVal);

    strKey = "last";
    const char *cVal = "Smith";
    obj.get_obj().emplace_back(strKey, cVal);

    strKey = "distance";
    obj.get_obj().emplace_back(strKey, (int64_t) 25);

    strKey = "time";
    obj.get_obj().emplace_back(strKey, (uint64_t) 3600);

    strKey = "calories";
    obj.get_obj().emplace_back(strKey, (int) 12);

    strKey = "temperature";
    obj.get_obj().emplace_back(strKey, (double) 90.012);

    BOOST_CHECK(!obj.empty());
    BOOST_CHECK_EQUAL(obj.size(), 7);

    strKey = "moon";
    obj.get_obj().emplace_back(strKey, "overwrite me");
    obj.get_obj().rbegin()->second = true;

    BOOST_CHECK(!obj.empty());
    BOOST_CHECK_EQUAL(obj.size(), 8);

    strKey = "spoon";
    obj.get_obj().emplace_back(strKey, false);
    obj.get_obj().push_back(std::make_pair(strKey, "just another spoon, but not the first one"));
    obj.get_obj().emplace_back(strKey, true);
    obj.get_obj().push_back(std::make_pair("spoon", "third spoon's a charm"));
    obj.get_obj().emplace_back("spoon", v);

    BOOST_CHECK(!obj.empty());
    BOOST_CHECK_EQUAL(obj.size(), 13);

    UniValue::Object obj2;
    // emplace with move constructor of std::pair
    obj2.emplace_back(std::make_pair<std::string, UniValue>("cat1", 8999));
    obj2.emplace_back("cat1", obj);
    obj2.emplace_back("cat1", 9000);
    // emplace with templated elementwise constructor of std::pair
    obj2.emplace_back(std::make_pair("cat2", 12345));

    BOOST_CHECK(!obj2.empty());
    BOOST_CHECK_EQUAL(obj2.size(), 4);

    for (auto pair = obj2.begin() + 2; pair != obj2.end(); ++pair) {
        obj.get_obj().push_back(std::move(*pair));
    }

    BOOST_CHECK(!obj.empty());
    BOOST_CHECK_EQUAL(obj.size(), 15);

    BOOST_CHECK_EQUAL(obj["age"].getValStr(), "100");
    BOOST_CHECK_EQUAL(obj["first"].getValStr(), "John");
    BOOST_CHECK_EQUAL(obj["last"].getValStr(), "Smith");
    BOOST_CHECK_EQUAL(obj["distance"].getValStr(), "25");
    BOOST_CHECK_EQUAL(obj["time"].getValStr(), "3600");
    BOOST_CHECK_EQUAL(obj["calories"].getValStr(), "12");
    BOOST_CHECK_EQUAL(obj["temperature"].getValStr(), "90.012");
    BOOST_CHECK_EQUAL(obj["moon"].getValStr(), "");
    BOOST_CHECK_EQUAL(obj["spoon"].getValStr(), ""); // checks the first spoon
    BOOST_CHECK_EQUAL(obj["cat1"].getValStr(), "9000");
    BOOST_CHECK_EQUAL(obj["cat2"].getValStr(), "12345");

    BOOST_CHECK_EQUAL(obj["nyuknyuknyuk"].getValStr(), "");

    // check all five spoons
    BOOST_CHECK(obj[8].isFalse());
    BOOST_CHECK_EQUAL(obj[9].getValStr(), "just another spoon, but not the first one");
    BOOST_CHECK(obj[10].isTrue());
    BOOST_CHECK_EQUAL(obj[11].getValStr(), "third spoon's a charm");
    BOOST_CHECK_EQUAL(obj[12].getValStr(), "100");

    BOOST_CHECK_EQUAL(&obj[0], &obj["age"]);
    BOOST_CHECK_EQUAL(&obj[1], &obj["first"]);
    BOOST_CHECK_EQUAL(&obj[2], &obj["last"]);
    BOOST_CHECK_EQUAL(&obj[3], &obj["distance"]);
    BOOST_CHECK_EQUAL(&obj[4], &obj["time"]);
    BOOST_CHECK_EQUAL(&obj[5], &obj["calories"]);
    BOOST_CHECK_EQUAL(&obj[6], &obj["temperature"]);
    BOOST_CHECK_EQUAL(&obj[7], &obj["moon"]);
    BOOST_CHECK_EQUAL(&obj[8], &obj["spoon"]);
    BOOST_CHECK_EQUAL(&obj[13], &obj["cat1"]);
    BOOST_CHECK_EQUAL(&obj[14], &obj["cat2"]);

    BOOST_CHECK_EQUAL(&obj[15], &UniValue::NullUniValue);

    BOOST_CHECK_EQUAL(obj.locate("age"), &obj["age"]);
    BOOST_CHECK_EQUAL(obj.locate("first"), &obj["first"]);
    BOOST_CHECK_EQUAL(obj.locate("last"), &obj["last"]);
    BOOST_CHECK_EQUAL(obj.locate("distance"), &obj["distance"]);
    BOOST_CHECK_EQUAL(obj.locate("time"), &obj["time"]);
    BOOST_CHECK_EQUAL(obj.locate("calories"), &obj["calories"]);
    BOOST_CHECK_EQUAL(obj.locate("temperature"), &obj["temperature"]);
    BOOST_CHECK_EQUAL(obj.locate("moon"), &obj["moon"]);
    BOOST_CHECK_EQUAL(obj.locate("spoon"), &obj["spoon"]);
    BOOST_CHECK_EQUAL(obj.locate("cat1"), &obj["cat1"]);
    BOOST_CHECK_EQUAL(obj.locate("cat2"), &obj["cat2"]);

    BOOST_CHECK_EQUAL(obj.locate("nyuknyuknyuk"), nullptr);

    BOOST_CHECK_EQUAL(&obj.at("age"), &obj["age"]);
    BOOST_CHECK_EQUAL(&obj.at("first"), &obj["first"]);
    BOOST_CHECK_EQUAL(&obj.at("last"), &obj["last"]);
    BOOST_CHECK_EQUAL(&obj.at("distance"), &obj["distance"]);
    BOOST_CHECK_EQUAL(&obj.at("time"), &obj["time"]);
    BOOST_CHECK_EQUAL(&obj.at("calories"), &obj["calories"]);
    BOOST_CHECK_EQUAL(&obj.at("temperature"), &obj["temperature"]);
    BOOST_CHECK_EQUAL(&obj.at("moon"), &obj["moon"]);
    BOOST_CHECK_EQUAL(&obj.at("spoon"), &obj["spoon"]);
    BOOST_CHECK_EQUAL(&obj.at("cat1"), &obj["cat1"]);
    BOOST_CHECK_EQUAL(&obj.at("cat2"), &obj["cat2"]);

    BOOST_CHECK_THROW(obj.at("nyuknyuknyuk"), std::out_of_range);

    for (int i = 0; i < 15; ++i)
        BOOST_CHECK_EQUAL(&obj.at(i), &obj[i]);

    BOOST_CHECK_THROW(obj.at(15), std::out_of_range);

    BOOST_CHECK_EQUAL(obj["age"].getType(), UniValue::VNUM);
    BOOST_CHECK_EQUAL(obj["first"].getType(), UniValue::VSTR);
    BOOST_CHECK_EQUAL(obj["last"].getType(), UniValue::VSTR);
    BOOST_CHECK_EQUAL(obj["distance"].getType(), UniValue::VNUM);
    BOOST_CHECK_EQUAL(obj["time"].getType(), UniValue::VNUM);
    BOOST_CHECK_EQUAL(obj["calories"].getType(), UniValue::VNUM);
    BOOST_CHECK_EQUAL(obj["temperature"].getType(), UniValue::VNUM);
    BOOST_CHECK_EQUAL(obj["moon"].getType(), UniValue::VTRUE);
    BOOST_CHECK_EQUAL(obj["spoon"].getType(), UniValue::VFALSE);
    BOOST_CHECK_EQUAL(obj["cat1"].getType(), UniValue::VNUM);
    BOOST_CHECK_EQUAL(obj["cat2"].getType(), UniValue::VNUM);

    BOOST_CHECK_EQUAL(obj[15].getType(), UniValue::VNULL);
    BOOST_CHECK_EQUAL(obj["nyuknyuknyuk"].getType(), UniValue::VNULL);

    // erase all spoons but the last
    auto after = obj.get_obj().erase(obj.get_obj().begin() + 8, obj.get_obj().begin() + 12);

    BOOST_CHECK(!obj.get_obj().empty());
    BOOST_CHECK_EQUAL(obj.get_obj().size(), 11);
    BOOST_CHECK_EQUAL(&obj.at(8), &obj.at("spoon"));
    BOOST_CHECK_EQUAL(&obj.at(9), &obj.at("cat1"));
    BOOST_CHECK_EQUAL(obj.at("spoon").getValStr(), "100");
    BOOST_CHECK_EQUAL(after->first, "spoon"); // the remaining spoon is after the removed spoons

    obj.setNull();
    BOOST_CHECK(obj.empty());
    BOOST_CHECK_EQUAL(obj.size(), 0);
    BOOST_CHECK_EQUAL(obj.getType(), UniValue::VNULL);
    BOOST_CHECK_EQUAL(&obj[0], &UniValue::NullUniValue);
    BOOST_CHECK_EQUAL(&obj["age"], &UniValue::NullUniValue);
    BOOST_CHECK_EQUAL(obj.locate("age"), nullptr);
    BOOST_CHECK_THROW(obj.at(0), std::domain_error);
    BOOST_CHECK_THROW(obj.at("age"), std::domain_error);

    obj.setObject();
    UniValue uv;
    uv = 42;
    obj.get_obj().emplace_back("age", uv);
    BOOST_CHECK_EQUAL(obj.size(), 1);
    BOOST_CHECK_EQUAL(obj["age"].getValStr(), "42");

    uv = 43;
    obj.get_obj().emplace_back("age", uv);
    BOOST_CHECK_EQUAL(obj.size(), 2);
    BOOST_CHECK_EQUAL(obj["age"].getValStr(), "42");

    obj.get_obj().emplace_back("name", "foo bar");
    BOOST_CHECK_EQUAL(obj["name"].getValStr(), "foo bar");

    // test front() / back() as well as operator==
    UniValue arr{UniValue::VNUM}; // this is intentional.
    BOOST_CHECK_EQUAL(&arr.front(), &UniValue::NullUniValue); // should return the NullUniValue if !array
    UniValue::Array vals;
    vals.emplace_back("foo");
    vals.emplace_back("bar");
    vals.emplace_back(UniValue::VOBJ);
    vals.emplace_back("baz");
    vals.emplace_back("bat");
    vals.emplace_back(false);
    vals.emplace_back();
    vals.emplace_back(1.2);
    vals.emplace_back(true);
    vals.emplace_back(10);
    vals.emplace_back(-42);
    vals.emplace_back(-12345678.11234678);
    vals.emplace_back(UniValue{UniValue::VARR});
    vals.at(2).get_obj().emplace_back("akey", "this is a value");
    *vals.rbegin() = vals; // vals recursively contains a partial copy of vals!
    const auto valsExpected(vals); // save a copy
    arr = std::move(vals); // assign to array via move
    BOOST_CHECK(vals.empty()); // vector should be empty after move
    BOOST_CHECK(!arr.empty()); // but our array should not be
    BOOST_CHECK(arr != UniValue{UniValue::VARR}); // check that UniValue::operator== is not a yes-man
    BOOST_CHECK(arr != UniValue{1.234}); // check operator== for differing types
    BOOST_CHECK_EQUAL(arr.front(), valsExpected.front());
    BOOST_CHECK_EQUAL(arr.back(), valsExpected.back());
    BOOST_CHECK(arr.get_array() == valsExpected);
}

static const char *json1 =
"[1.10000000,{\"key1\":\"str\\u0000\",\"key2\":800,\"key3\":{\"name\":\"martian http://test.com\"}}]";

BOOST_AUTO_TEST_CASE(univalue_readwrite)
{
    UniValue v;
    BOOST_CHECK(v.read(json1));
    const UniValue vjson1(v); // save a copy for below

    std::string strJson1(json1);
    BOOST_CHECK(v.read(strJson1));

    BOOST_CHECK(v.isArray());
    BOOST_CHECK_EQUAL(v.size(), 2);

    BOOST_CHECK_EQUAL(v[0].getValStr(), "1.10000000");

    const UniValue& obj = v[1];
    BOOST_CHECK(obj.isObject());
    BOOST_CHECK_EQUAL(obj.size(), 3);

    BOOST_CHECK(obj["key1"].isStr());
    std::string correctValue("str");
    correctValue.push_back('\0');
    BOOST_CHECK_EQUAL(obj["key1"].getValStr(), correctValue);
    BOOST_CHECK(obj["key2"].isNum());
    BOOST_CHECK_EQUAL(obj["key2"].getValStr(), "800");
    BOOST_CHECK(obj["key3"].isObject());

    BOOST_CHECK_EQUAL(strJson1, UniValue::stringify(v));

    /* Check for (correctly reporting) a parsing error if the initial
       JSON construct is followed by more stuff.  Note that whitespace
       is, of course, exempt.  */

    BOOST_CHECK(v.read("  {}\n  "));
    BOOST_CHECK(v.isObject());
    BOOST_CHECK(v.read("  []\n  "));
    BOOST_CHECK(v.isArray());

    BOOST_CHECK(!v.read("@{}"));
    BOOST_CHECK(!v.read("{} garbage"));
    BOOST_CHECK(!v.read("[]{}"));
    BOOST_CHECK(!v.read("{}[]"));
    BOOST_CHECK(!v.read("{} 42"));

    // check that json escapes work correctly by putting a json string INTO a UniValue
    // and doing a round of ser/deser on it.
    v.setArray();
    v.get_array().emplace_back(json1);
    const UniValue vcopy(v);
    BOOST_CHECK(!vcopy.empty());
    v.setNull();
    BOOST_CHECK(v.empty());
    BOOST_CHECK(v.read(UniValue::stringify(vcopy, 2)));
    BOOST_CHECK(!v.empty());
    BOOST_CHECK_EQUAL(v, vcopy);
    BOOST_CHECK_EQUAL(v[0], json1);
    v.setNull();
    BOOST_CHECK(v.empty());
    BOOST_CHECK(v.read(vcopy[0].get_str())); // now deserialize the embedded json string
    BOOST_CHECK(!v.empty());
    BOOST_CHECK_EQUAL(v, vjson1); // ensure it deserializes to equal
}

BOOST_AUTO_TEST_SUITE_END()

int main()
{
    try {
        // First, we try to set the locale from the "user preferences" (typical env vars LC_ALL and/or LANG).
        // We do this for CI/testing setups that want to run this test on one of the breaking locales
        // such as "de_DE.UTF-8".
        std::setlocale(LC_ALL, "");
        std::locale loc("");
        std::locale::global(loc);
    } catch (...) {
        // If the env var specified a locale that does not exist or was installed, we can end up here.
        // Just fallback to the standard "C" locale in that case.
        std::setlocale(LC_ALL, "C");
        std::locale loc("C");
        std::locale::global(loc);
    }

    univalue_constructor();
    univalue_typecheck();
    univalue_set();
    univalue_array();
    univalue_object();
    univalue_readwrite();
    return 0;
}
