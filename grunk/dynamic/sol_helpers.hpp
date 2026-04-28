// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "grunk/dynamic/object.hpp"
#include <regex>

namespace grunk {

namespace details {

    /**
 * @brief Accesses a nested element in a sol::table by traversing keys separated by dots (.) or colons (:).
 *
 * @param table The sol::table to be traversed.
 * @param str The string representing the path of nested keys, separated by dots (.) or colons (:).
 * @return sol::object The nested object in the table at the specified path.
 *
 * This function splits the string `str` at each dot (.) or colon (:), then uses each part as a key for accessing
 * nested tables in `table`. If any key is invalid or does not exist, an exception is thrown.
 *
 * @note This function assumes the table contains only sol::table elements at each nested level except for the final key.
 * If any key is invalid or does not exist, an exception is thrown.
 */
inline sol::object lookup_nested(sol::table const& table, std::string const& str) {
    sol::object current = table;

    // Use regex to split by both '.' and ':'
    std::regex delimiter_regex(R"([.:])");
    std::sregex_token_iterator iter(str.begin(), str.end(), delimiter_regex, -1);
    std::sregex_token_iterator end;

    for (; iter != end; ++iter) {
        std::string key = *iter;

        // Ensure current object is a table before accessing the next key
        if (current.get_type() != sol::type::table) {
            std::string error = std::string("Could not resolve \"") + key +
                                "\" in identifier \"" + str +
                                "\". Are all types properly registered in the grunk state?";
            throw std::runtime_error(error);
        }

        current = current.as<sol::table>()[key];
    }

    return current;
}

} // namespace details

} // namespace grunk