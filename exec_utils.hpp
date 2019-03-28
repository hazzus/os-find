#ifndef EXECUTIL_OSFIND
#define EXECUTIL_OSFIND

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

void error(std::string const &cause) {
  std::cerr << cause << strerror(errno) << std::endl;
}

char *converter(std::string const &s) { return const_cast<char *>(s.data()); }

std::vector<char *> get_ptrs(std::vector<std::string> &args) {
  std::vector<char *> result;
  std::transform(args.begin(), args.end(), std::back_inserter(result),
                 converter);
  return result;
}

void execute(char *argv[]) {
  switch (auto pid = fork()) {
  case -1:
    error("Can not fork");
    break;
  case 0:
    if (execv(argv[0], argv) == -1) {
      error("Execution failed");
      exit(-1);
    }
    exit(0);
  default:
    int status;
    if (waitpid(pid, &status, 0) == -1) {
      error("Error in execution");
    } else {
      std::cout << "Executed. Return code: " << WEXITSTATUS(status)
                << std::endl;
    }
  }
}

void pre_execute(std::vector<std::string> &args) {
  auto prepared = get_ptrs(args);
  prepared.push_back(nullptr);
  execute(prepared.data());
}

#endif
