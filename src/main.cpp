#include "parser.hpp"
#include "repl.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <unistd.h>

#ifndef FPC_VERSION
#define FPC_VERSION "0.0.0"
#endif

namespace {
void print_help() {
  std::cout <<
    "fpc " << FPC_VERSION << "\n"
    "usage:\n"
    "  fpc                         start interactive REPL\n"
    "  fpc -e <expr>               evaluate one expression or command\n"
    "  fpc <expr>                  evaluate one expression or command\n"
    "  fpc <script.fpc>            run script file\n"
    "  fpc --help, -h              print this help\n"
    "  fpc --version, -v           print version\n"
    "\n"
    << help_text() << "\n";
}
}

int main(int argc, char** argv) {
  Session session;
  if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
    print_help();
    return 0;
  }
  if (argc == 2 && (std::string(argv[1]) == "--version" || std::string(argv[1]) == "-v")) {
    std::cout << "fpc " << FPC_VERSION << "\n";
    return 0;
  }
  if (argc >= 3 && std::string(argv[1]) == "-e") {
    auto r = execute_line(argv[2], session);
    if (r.has_output) std::cout << (r.is_error ? "error: " : "") << r.output << "\n";
    return r.is_error ? 1 : 0;
  }
  if (argc >= 2) {
    if (argc == 2) {
      std::string arg = argv[1];
      if (std::ifstream(arg).good()) return run_file(arg, session);
      auto r = execute_line(arg, session);
      if (r.has_output) std::cout << (r.is_error ? "error: " : "") << r.output << "\n";
      return r.is_error ? 1 : 0;
    }
  }
  if (!isatty(STDIN_FILENO)) return run_stream(std::cin, session, false);
  return run_repl(session);
}
