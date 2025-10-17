#pragma once

#include <stdexcept>
#include <string>

namespace grunk {

using namespace std::string_literals;

/**
 * @brief An exception representing errors with grunk's file I/O system
 */
class io_error : public std::exception
{
public: 
    /**
     * @brief Construct a new io error object from an error message
     * 
     * @param msg 
     */
    io_error(std::string const& msg);

    /**
     * @brief print the error message with the prefix "grunk IO error"
     * 
     * @return const char* the error message
     */
    const char *what() const noexcept override;

    /**
     * @brief Get the error message without the prefix "grunk IO error"
     * 
     * @return std::string the error message
     */
    std::string get_message() const;
private:
    std::string mMessage;
};

} //namespace grunk
