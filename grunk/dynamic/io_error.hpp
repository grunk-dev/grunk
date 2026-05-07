// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <stdexcept>
#include <string>

namespace grunk {

using namespace std::string_literals;

/**
 * @brief An exception representing errors with grunk's file I/O system
 *
 * @ingroup advanced_dynamic
 */
class io_error : public std::exception
{
public: 
    /**
     * @brief Construct a new io error object from an error message
     * 
     * @param msg 
     */
    inline io_error(std::string const& msg)
    : mMessage("grunk IO error: "s + msg)
    {}

    /**
     * @brief print the error message with the prefix "grunk IO error"
     * 
     * @return const char* the error message
     */
    inline const char *what() const noexcept override
    {
        return mMessage.c_str();
    }

    /**
     * @brief Get the error message without the prefix "grunk IO error"
     * 
     * @return std::string the error message
     */
    std::string get_message() const
    {
        return mMessage;
    }

private:
    std::string mMessage;
};

} //namespace grunk
