#pragma once

#include "session.hpp"

#include <iosfwd>
#include <string>

bool colors_enabled(const Session& session, bool interactive);
int run_repl(Session& session);
int run_stream(std::istream& input, Session& session, bool color);
int run_file(const std::string& path, Session& session);
