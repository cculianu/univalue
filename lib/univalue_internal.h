// Copyright (c) 2021 Calin A. Culianu <calin.culianu@gmail.com>
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <optional>
#include <string>

/// Definitions and functions used internally by the UniValue library
namespace univalue_internal {
extern std::optional<std::string> validateAndStripNumStr(const char* s);
extern bool ParseInt(const std::string& str, int *out) noexcept;
extern bool ParseUInt(const std::string& str, unsigned *out) noexcept;
extern bool ParseInt64(const std::string& str, int64_t *out) noexcept;
extern bool ParseUInt64(const std::string& str, uint64_t *out) noexcept;
extern bool ParseDouble(const std::string& str, double *out);
} // namespace univalue_internal
