// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <fstream>
#include <functional>
#include <string>
#include <vector>

#ifndef OPENVINO_ENABLE_UNICODE_PATH_SUPPORT
#    ifdef _WIN32
#        if defined __INTEL_COMPILER || defined _MSC_VER
#            define OPENVINO_ENABLE_UNICODE_PATH_SUPPORT
#        endif
#    elif defined(__GNUC__) && (__GNUC__ > 5 || (__GNUC__ == 5 && __GNUC_MINOR__ > 2)) || defined(__clang__)
#        define OPENVINO_ENABLE_UNICODE_PATH_SUPPORT
#    endif
#endif

namespace ov {
namespace util {

/// OS specific file traits
template <class C>
struct FileTraits;

template <>
struct FileTraits<char> {
    static constexpr const auto file_separator =
#ifdef _WIN32
        '\\';
#else
        '/';
#endif
    static constexpr const auto dot_symbol = '.';
    static std::string library_ext() {
#ifdef _WIN32
        return {"dll"};
#else
        return {"so"};
#endif
    }
    static std::string library_prefix() {
#ifdef _WIN32
        return {""};
#else
        return {"lib"};
#endif
    }
};

template <>
struct FileTraits<wchar_t> {
    static constexpr const auto file_separator =
#ifdef _WIN32
        L'\\';
#else
        L'/';
#endif
    static constexpr const auto dot_symbol = L'.';
    static std::wstring library_ext() {
#ifdef _WIN32
        return {L"dll"};
#else
        return {L"so"};
#endif
    }
    static std::wstring library_prefix() {
#ifdef _WIN32
        return {L""};
#else
        return {L"lib"};
#endif
    }
};

#ifdef OPENVINO_ENABLE_UNICODE_PATH_SUPPORT
/**
 * @brief Conversion from wide character string to a single-byte chain.
 * @param wstr A wide-char string
 * @return A multi-byte string
 */
std::string wstring_to_string(const std::wstring& wstr);
/**
 * @brief Conversion from single-byte chain to wide character string.
 * @param str A null-terminated string
 * @return A wide-char string
 */
std::wstring string_to_wstring(const std::string& str);

#endif

/// \brief Remove path components which would allow traversing up a directory tree.
/// \param path A path to file
/// \return A sanitiazed path
std::string sanitize_path(const std::string& path);

/// \brief Returns the name with extension for a given path
/// \param path The path to the output file
std::string get_file_name(const std::string& path);

/**
 * @brief Interface function to get absolute path of file
 * @param path - path to file, can be relative to current working directory
 * @return Absolute path of file
 * @throw runtime_exception if any error occurred
 */
std::string get_absolute_file_path(const std::string& path);
/**
 * @brief Interface function to create directorty recursively by given path
 * @param path - path to file, can be relative to current working directory
 * @throw runtime_exception if any error occurred
 */
void create_directory_recursive(const std::string& path);

/**
 * @brief Interface function to check if directory exists for given path
 * @param path - path to directory
 * @return true if directory exists, false otherwise
 */
bool directory_exists(const std::string& path);

/**
 * @brief      Returns file size for file
 * @param[in]  path  The file name
 * @return     file size
 */
inline uint64_t file_size(const char* path) {
#if defined(OPENVINO_ENABLE_UNICODE_PATH_SUPPORT) && defined(_WIN32)
    std::wstring widefilename = ov::util::string_to_wstring(path);
    const wchar_t* file_name = widefilename.c_str();
#elif defined(__ANDROID__) || defined(ANDROID)
    std::string file_name = path;
    std::string::size_type pos = file_name.find('!');
    if (pos != std::string::npos) {
        file_name = file_name.substr(0, pos);
    }
#else
    const char* file_name = path;
#endif
    std::ifstream in(file_name, std::ios_base::binary | std::ios_base::ate);
    return in.tellg();
}

#ifdef OPENVINO_ENABLE_UNICODE_PATH_SUPPORT

/**
 * @brief      Returns file size for file
 * @param[in]  path  The file name
 * @return     file size
 */
inline uint64_t file_size(const std::wstring& path) {
    return file_size(wstring_to_string(path).c_str());
}

#endif  // OPENVINO_ENABLE_UNICODE_PATH_SUPPORT

/**
 * @brief      Returns file size for file
 * @param[in]  path  The file name
 * @return     file size
 */
inline uint64_t file_size(const std::string& path) {
    return file_size(path.c_str());
}

/**
 * @brief      Returns true if file exists
 * @param[in]  path  The path to file
 * @return     true if file exists
 */
template <typename C,
          typename = typename std::enable_if<(std::is_same<C, char>::value || std::is_same<C, wchar_t>::value)>::type>
inline bool file_exists(const std::basic_string<C>& path) {
    return file_size(path) > 0;
}

std::string get_file_ext(const std::string& path);
std::string get_directory(const std::string& path);
std::string path_join(const std::vector<std::string>& paths);

void iterate_files(const std::string& path,
                   const std::function<void(const std::string& file, bool is_dir)>& func,
                   bool recurse = false,
                   bool include_links = false);

void convert_path_win_style(std::string& path);

std::string get_ov_lib_path();

#ifdef OPENVINO_ENABLE_UNICODE_PATH_SUPPORT

using FilePath = std::wstring;

inline std::string from_file_path(const FilePath& path) {
    return wstring_to_string(path);
}

inline FilePath to_file_path(const std::string& path) {
    return string_to_wstring(path);
}

#else

using FilePath = std::string;

inline std::string from_file_path(const FilePath& path) {
    return path;
}

inline FilePath to_file_path(const std::string& path) {
    return path;
}

#endif  // OPENVINO_ENABLE_UNICODE_PATH_SUPPORT

#ifdef OPENVINO_ENABLE_UNICODE_PATH_SUPPORT

/**
 * @brief   Returns a unicode path to openvino libraries
 * @return  A `std::wstring` path to openvino libraries
 */
std::wstring get_ov_lib_path_w();

inline std::wstring get_ov_library_path() {
    return get_ov_lib_path_w();
}

#else

inline std::string get_ov_library_path() {
    return get_ov_lib_path();
}

#endif  // OPENVINO_ENABLE_UNICODE_PATH_SUPPORT

template <typename C,
          typename = typename std::enable_if<(std::is_same<C, char>::value || std::is_same<C, wchar_t>::value)>::type>
inline std::basic_string<C> make_plugin_library_name(const std::basic_string<C>& path,
                                                     const std::basic_string<C>& input) {
    std::basic_string<C> separator(1, FileTraits<C>::file_separator);
    if (path.empty())
        separator = {};
    return path + separator + FileTraits<C>::library_prefix() + input + FileTraits<C>::dot_symbol +
           FileTraits<C>::library_ext();
}

}  // namespace util
}  // namespace ov
