#include <grunk/common/common_functions.hpp>

#include <stdlib.h>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <shlobj.h>
#pragma comment(lib, "shell32.lib")
#else 
#include <unistd.h>
#include <pwd.h>
#endif

namespace grunk {

std::string get_home_dir() {
    std::string ret;

#if defined(_WIN32) || defined(_WIN64)
    char path[MAX_PATH];
    if(SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, path))) {
        ret = path;
    }
#else 
    const char* home = std::getenv("HOME");
    if (home == nullptr) {
        home = getpwuid(getuid())->pw_dir;
    }
    ret = home;
#endif

    return ret;
}

std::vector<std::string> split(std::string input, std::string delimiter)
{
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    std::string token;
    std::vector<std::string> res;

    while ((pos_end = input.find(delimiter, pos_start)) != std::string::npos) {
        token = input.substr (pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back (token);
    }

    res.push_back (input.substr (pos_start));
    return res;
}


} // namespace grunk