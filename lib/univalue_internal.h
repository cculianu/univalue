#pragma once

#include <optional>
#include <string>

/// Definitions and functions used internally by the UniValue library
namespace univalue_internal {
extern std::optional<std::string> validateAndStripNumStr(const char* s);
extern bool ParseInt32(const std::string& str, int32_t *out) noexcept;
extern bool ParseInt64(const std::string& str, int64_t *out) noexcept;
extern bool ParseDouble(const std::string& str, double *out);
} // namespace univalue_internal
