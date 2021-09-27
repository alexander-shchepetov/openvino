// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <algorithm>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace ov {
namespace util {

template <typename T>
std::string join(const T& v, const std::string& sep = ", ") {
    std::ostringstream ss;
    size_t count = 0;
    for (const auto& x : v) {
        if (count++ > 0) {
            ss << sep;
        }
        ss << x;
    }
    return ss.str();
}

template <typename T>
std::string vector_to_string(const T& v) {
    std::ostringstream os;
    os << "[ " << ov::util::join(v) << " ]";
    return os.str();
}

std::string to_lower(const std::string& s);

std::string to_upper(const std::string& s);

/**
 * @brief trim from start (in place)
 * @param s - string to trim
 */
inline std::string ltrim(const std::string& s) {
    std::string ret = s;
    ret.erase(ret.begin(), std::find_if(ret.begin(), ret.end(), [](int c) {
                  return !std::isspace(c);
              }));
    return ret;
}

/**
 * @brief trim from end (in place)
 * @param s - string to trim
 */
inline std::string rtrim(const std::string& s) {
    std::string ret = s;
    ret.erase(std::find_if(ret.rbegin(),
                           ret.rend(),
                           [](int c) {
                               return !std::isspace(c);
                           })
                  .base(),
              ret.end());
    return ret;
}

/**
 * @brief Trims std::string from both ends (in place)
 * @ingroup ie_dev_api_error_debug
 * @param s A reference to a std::tring to trim
 * @return A reference to a trimmed std::string
 */
inline std::string trim(const std::string& s) {
    std::string ret = ltrim(s);
    ret = rtrim(ret);
    return ret;
}

/**
 * @brief check string end with given substring
 * @param src - string to check
 * @param with - given substring
 * @return true if string end with given substring
 */
inline bool ends_with(const std::string& src, const char* with) {
    int wl = static_cast<int>(strlen(with));
    int so = static_cast<int>(src.length()) - wl;
    if (so < 0)
        return false;
    return 0 == strncmp(with, &src[so], wl);
}

std::vector<std::string> split(const std::string& s, char delimiter, bool trim = false);

template <typename T>
T ceil_div(const T& x, const T& y) {
    return (x == 0 ? 0 : (1 + (x - 1) / y));
}
}  // namespace util
}  // namespace ov
