#ifndef PARSEUTILS_OSFIND
#define PARSEUTILS_OSFIND

#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>
#include <numeric>

namespace args {

// clang-format off
static std::map<std::string, std::string> options = {
    {"help",           "show help"},
    {"path",           "(first argument) path to directory"},
    {"inum",           "inode number"},
    {"name",           "name of file"},
    {"size",           "[-=+] size of file]"},
    {"nlinks",         "number of hardlinks"},
    {"exec",           "path to execution file"}
};
// clang-format on

std::pair<std::string, std::string> parse_arg(char *arg) {
    std::string arg_str = arg;
    for (size_t i = 0; i < arg_str.length(); i++) {
        if (arg_str[i] == '=') {
            std::string value = arg_str.substr(i + 1);
            if (value.front() == value.back() &&
                (value.front() == '\'' || value.front() == '\"'))
                value = value.substr(1, value.length() - 2);
            return {arg_str.substr(1, i - 1), value};
        }
    }
    return {arg_str.substr(1), ""};
}

std::map<std::string, std::string> parse(int const &argc, char *argv[]) {
    std::map<std::string, std::string> result;
    std::string first_arg = argv[1];
    if (first_arg == "-help") {
        result.emplace("help", "");
    } else {
        result.emplace("path", std::string(argv[1]));
    }
    for (int i = 2; i < argc; i++) {
        result.insert(parse_arg(argv[i]));
    }
    return result;
}

bool check(std::map<std::string, std::string> const &args) {
    for (auto &&arg_pair : args) {
        if (!options.count(arg_pair.first)) {
            std::cerr << "Unexpected key: -" << arg_pair.first << std::endl;
            return false;
        }
    }
    return true;
}

std::string help() {
    std::string result = std::accumulate(
        options.begin(), options.end(), std::string("Options:\n"),
        [](std::string a, std::pair<std::string, std::string> p) {
            return std::move(a) + "-" + p.first + ": " + p.second + "\n";
        });
    return result;
}

} // namespace args

#endif
