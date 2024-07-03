
#pragma once 

#include <string>
#include <vector>

namespace grunk {

/**
 * @brief gets the HOME directory programmatically. 
 *
 * This should hopefully work for Windows, MacOS and Linux
 * 
 * @return std::string The home directory
 */
std::string get_home_dir();

/**
 * @brief splits a string at a given delimiter and returns the splitted string as a vector
 * 
 * @param input string to split
 * @param delimiter delimiter, where to split
 * @return std::vector<std::string> 
 */
std::vector<std::string> split(std::string input, std::string delimiter);

} // namespace grunk