// Copyright 2014 BitPay Inc.
// Copyright 2015 Bitcoin Core Developers
// Copyright (c) 2020-2021 The Bitcoin developers
// Copyright (c) 2021 Calin A. Culianu <calin.culianu@gmail.com>
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "univalue.h"
#include "univalue_internal.h"

#include <stdexcept>

bool UniValue::get_bool() const
{
    if (!isBool())
        throw std::runtime_error("JSON value is not a boolean as expected");
    return getBool();
}

int UniValue::get_int() const
{
    if (!isNum())
        throw std::runtime_error("JSON value is not an integer as expected");
    int retval;
    if (!univalue_internal::ParseInt(getValStr(), &retval))
        throw std::runtime_error("JSON integer out of range");
    return retval;
}

unsigned UniValue::get_uint() const
{
    if (!isNum())
        throw std::runtime_error("JSON value is not an integer as expected");
    unsigned retval;
    if (!univalue_internal::ParseUInt(getValStr(), &retval))
        throw std::runtime_error("JSON unsigned integer out of range");
    return retval;
}

int64_t UniValue::get_int64() const
{
    if (!isNum())
        throw std::runtime_error("JSON value is not an integer as expected");
    int64_t retval;
    if (!univalue_internal::ParseInt64(getValStr(), &retval))
        throw std::runtime_error("JSON integer out of range");
    return retval;
}

uint64_t UniValue::get_uint64() const
{
    if (!isNum())
        throw std::runtime_error("JSON value is not an integer as expected");
    uint64_t retval;
    if (!univalue_internal::ParseUInt64(getValStr(), &retval))
        throw std::runtime_error("JSON unsigned integer out of range");
    return retval;
}

double UniValue::get_real() const
{
    if (!isNum())
        throw std::runtime_error("JSON value is not a number as expected");
    double retval;
    if (!univalue_internal::ParseDouble(getValStr(), &retval))
        throw std::runtime_error("JSON double out of range");
    return retval;
}

const std::string& UniValue::get_str() const
{
    if (!isStr())
        throw std::runtime_error("JSON value is not a string as expected");
    return u.val;
}
std::string& UniValue::get_str()
{
    if (!isStr())
        throw std::runtime_error("JSON value is not a string as expected");
    return u.val;
}

const UniValue::Object& UniValue::get_obj() const
{
    if (!isObject())
        throw std::runtime_error("JSON value is not an object as expected");
    return u.entries;
}
UniValue::Object& UniValue::get_obj()
{
    if (!isObject())
        throw std::runtime_error("JSON value is not an object as expected");
    return u.entries;
}

const UniValue::Array& UniValue::get_array() const
{
    if (!isArray())
        throw std::runtime_error("JSON value is not an array as expected");
    return u.values;
}
UniValue::Array& UniValue::get_array()
{
    if (!isArray())
        throw std::runtime_error("JSON value is not an array as expected");
    return u.values;
}
