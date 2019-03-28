#include <boost/program_options.hpp>
#include <deque>
#include <iostream>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "exec_utils.hpp"

namespace po = boost::program_options;

struct stat_predicate {
  stat_predicate(po::variables_map const &vars_map) {
    if (vars_map.count("inum")) {
      inode_predicate = actual_predicate<ino_t>(vars_map["inum"].as<ino_t>(),
                                                equivalent<ino_t>);
    }
    if (vars_map.count("name")) {
      name_predicate = actual_predicate<const char *>(
          vars_map["name"].as<std::string>().c_str(),
          [](char const *one, char const *two) {
            return std::strcmp(one, two) == 0;
          });
    }
    if (vars_map.count("nlinks")) {
      nlink_predicate = actual_predicate<nlink_t>(
          vars_map["nlinks"].as<nlink_t>(), equivalent<nlink_t>);
    }
    if (vars_map.count("size")) {
      std::string size_control = vars_map["size"].as<std::string>();
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
  po::positional_options_description required;
  required.add("path", -1);
  po::options_description allowed("Allowed options");
  // clang-format off
  allowed.add_options()
          ("help,h", "Help options")
          ("path",   po::value<std::string>(), "Path to directory that should be scanned")
          ("inum",   po::value<ino_t>(),       "Number of inode")
          ("name",   po::value<std::string>(), "Name of file")
          ("size",   po::value<std::string>(), "[+=-] Control size of file")
          ("nlinks", po::value<nlink_t>(),     "Amount of hardlinks at file")
          ("exec",   po::value<std::string>(), "Path to program to execute with result file as single parameter");
  // clang-format on
  po::command_line_parser parser{argc, argv};
  parser.positional(required).options(allowed);
  po::variables_map vars_map;
  try {
    store(parser.run(), vars_map);
  } catch (const po::error &e) {
    std::cerr << e.what() << std::endl;
    return -1;
  }

  if (!vars_map.count("path")) {
    std::cerr << "No path to dir. Exiting";
    return -1;
  }

  if (vars_map.count("help")) {
    std::cout << "First argument must be path to directory where to search"
              << std::endl;
    std::cout << allowed;
    return 0;
  }

  stat_predicate predicate(vars_map);
  auto result = find(vars_map["path"].as<std::string>(), predicate);

  for (std::string const &file : result) {
    std::cout << file << std::endl;
  }

  if (vars_map.count("exec")) {
    pre_execute(result);
  }

  return 0;
}
