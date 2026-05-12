#include "repl.hpp"

#include "parser.hpp"

#include <readline/history.h>
#include <readline/readline.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <memory>

bool colors_enabled(const Session& session, bool interactive) {
  if (session.color == ColorMode::Always) return true;
  if (session.color == ColorMode::Never) return false;
  return interactive && isatty(STDOUT_FILENO);
}

namespace {
void print_line(const LineResult& r, bool color) {
  if (!r.has_output) return;
  if (r.is_error) {
    if (color) std::cout << "\033[1;31merror: " << r.output << "\033[0m\n";
    else std::cout << "error: " << r.output << "\n";
  } else {
    if (color) std::cout << "\033[1m" << r.output << "\033[0m\n";
    else std::cout << r.output << "\n";
  }
}
}

int run_repl(Session& session) {
  rl_variable_bind("editing-mode", "vi");
  while (true) {
    const bool color = colors_enabled(session, true);
    std::string prompt = statusline(session, color);
    std::unique_ptr<char, decltype(&std::free)> line(readline(prompt.c_str()), &std::free);
    if (!line) break;
    std::string s(line.get());
    if (!s.empty()) add_history(line.get());
    auto r = execute_line(s, session);
    print_line(r, colors_enabled(session, true));
    if (r.quit) break;
  }
  return 0;
}

int run_stream(std::istream& input, Session& session, bool color) {
  std::string line;
  int status = 0;
  while (std::getline(input, line)) {
    auto r = execute_line(line, session);
    print_line(r, colors_enabled(session, false));
    if (r.is_error) status = 1;
    if (r.quit) break;
  }
  return status;
}

int run_file(const std::string& path, Session& session) {
  std::ifstream file(path);
  if (!file) {
    std::cerr << "error: cannot open " << path << "\n";
    return 1;
  }
  return run_stream(file, session, false);
}
