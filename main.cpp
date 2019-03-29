#include <deque>
#include <functional>
#include <iostream>
#include <map>
#include <vector>

#include <cstring>
#include <dirent.h>
#include <memory.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "exec_utils.hpp"
#include "parse_utils.hpp"

struct stat_predicate {
  stat_predicate(std::map<std::string, std::string> const &vars_map) {
    if (vars_map.count("inum")) {
      inode_predicate = actual_predicate<ino_t>(
          static_cast<ino_t>(stoll(vars_map.at("inum"))), equivalent<ino_t>);
    }
    if (vars_map.count("name")) {
      name_predicate = actual_predicate<const char *>(
          vars_map.at("name").c_str(), [](char const *one, char const *two) {
            return std::strcmp(one, two) == 0;
          });
    }
    if (vars_map.count("nlinks")) {
      nlink_predicate = actual_predicate<nlink_t>(
          static_cast<nlink_t>(stoll(vars_map.at("nlinks"))),
          equivalent<nlink_t>);
    }
    if (vars_map.count("size")) {
      std::string size_control = vars_map.at("size");
      try {
        off_t needed_size = static_cast<off_t>(stoi(size_control.substr(1)));
        if (size_control.front() == '=') {
          size_predicate =
              actual_predicate<off_t>(needed_size, equivalent<off_t>);
        } else if (size_control.front() == '-') {
          size_predicate = actual_predicate<off_t>(
              needed_size,
              [](off_t const &one, off_t const &two) { return one < two; });
        } else if (size_control.front() == '+') {
          size_predicate = actual_predicate<off_t>(
              needed_size,
              [](off_t const &one, off_t const &two) { return one > two; });
        } else {
          std::cerr << "Incorrect size key, ignoring it";
        }
      } catch (std::invalid_argument &) {
        std::cerr << "Invalid size argument, ignoring it";
      }
    }
  }

  bool operator()(char *filename, struct stat const &fileinfo) {
    // clang-format off
    return inode_predicate(fileinfo.st_ino) &&
           name_predicate(filename) &&
           nlink_predicate(fileinfo.st_nlink) &&
           size_predicate(fileinfo.st_size);
    // clang-format on
  }

private:
  template <typename T> static bool true_predicate(T) { return true; }

  template <typename T> static bool equivalent(T const &one, T const &two) {
    return one == two;
  }

  template <typename T> struct actual_predicate {
    T needed;
    std::function<bool(T, T)> comparator;
    actual_predicate(T const &n, std::function<bool(T, T)> const &c)
        : needed(n), comparator(c) {}
    bool operator()(T const &real) { return comparator(real, needed); }
  };

  std::function<bool(ino_t)> inode_predicate = true_predicate<ino_t>;
  std::function<bool(const char *)> name_predicate =
      true_predicate<const char *>;
  std::function<bool(nlink_t)> nlink_predicate = true_predicate<nlink_t>;
  std::function<bool(off_t)> size_predicate = true_predicate<off_t>;
};

bool is_dots(char const *filename) {
  return (strcmp(filename, ".") == 0) || (strcmp(filename, "..") == 0);
}

// bfs traversal
std::vector<std::string>
find(std::string dir_path, std::function<bool(char *, struct stat)> predicate) {
  std::vector<std::string> result;
  auto directory = opendir(dir_path.c_str());
  std::deque<std::pair<DIR *, std::string>> dir_queue;
  dir_queue.push_back({directory, dir_path});
  while (!dir_queue.empty()) {
    auto current_dir = dir_queue.front();
    dir_queue.pop_front();
    if (current_dir.first == nullptr) {
      error("Error attempting to open: " + current_dir.second);
      continue;
    }
    while (auto file = readdir(current_dir.first)) {
      auto filename = file->d_name;
      if (!filename || is_dots(filename))
        continue;
      std::string filepath = current_dir.second + "/" + filename;

      struct stat buffer;
      if (lstat(filepath.c_str(), &buffer) == -1) {
        error(std::string("Could not open file: ") + filename);
        continue;
      }

      if (S_ISDIR(buffer.st_mode)) {
        dir_queue.emplace_back(opendir(filepath.c_str()), filepath);
      } else if (predicate(filename, buffer)) {
        result.push_back(filepath);
      }
    }
    closedir(current_dir.first);
  }
  return result;
}

int main(int argc, char *argv[]) {
  auto vars_map = args::parse(argc, argv);
  if (!args::check(vars_map)) {
    return -1;
  }

  if (vars_map.count("help")) {
    std::cout << args::help();
    return 0;
  }

  if (!vars_map.count("path")) {
    std::cerr << "No path to dir. Exiting";
    return -1;
  }

  stat_predicate predicate(vars_map);
  auto result = find(vars_map["path"], predicate);

  for (std::string const &file : result) {
    std::cout << file << std::endl;
  }

  if (vars_map.count("exec")) {
    pre_execute(result);
  }

  return 0;
}
