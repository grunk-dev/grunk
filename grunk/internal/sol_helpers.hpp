#pragma once

#include "function_traits.hpp"
#include "object.hpp"

#include <regex>

namespace grunk {


/**
 * @brief get_metadata retrieves a value stored in the metatable of a LUA object
 * @param obj the LUA object
 * @param key The key in the metatable
 */
template <typename U>
object get_metadata(sol::state_view& lua, U& obj, std::string const& key)
{
    //TODO: This seems like an inconvenient way to get the metatable. But
    // unfortunately I haven't found a sol2 API function to retrieve the metatable of a
    // function
    lua["_tmp"] = std::forward<U>(obj);
    lua.script(R"(
        _tmp = getmetatable(_tmp)
    )");

    return lua["_tmp"][key];;
}

template <typename U, typename F>
inline void set_function(U& obj, std::string const& key, F&& fun)
{
    obj.set_function(key, std::forward<F>(fun));
    auto funobj = obj[key];
    if (!funobj[sol::metatable_key].valid()) {
        sol::state_view lua(obj.lua_state());
        sol::table func_meta = lua.create_table();
        funobj[sol::metatable_key] = func_meta;
    }
    funobj[sol::metatable_key]["name"] = key;
    funobj[sol::metatable_key]["is_pure"] = details::function_traits<F>::is_pure;
}

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


} // namespace grunk
