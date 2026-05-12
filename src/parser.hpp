#pragma once

#include "fixed.hpp"

#include <string>

struct LineResult {
  bool quit = false;
  bool has_output = false;
  bool is_error = false;
  std::string output;
};

LineResult execute_line(const std::string& line, Session& session);
EvalResult eval_expression(const std::string& text, const Session& session);
std::string help_text();
