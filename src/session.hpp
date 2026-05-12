#pragma once

#include <gmpxx.h>
#include <string>

enum class RoundingMode { Trunc, Zero, Floor, Ceil, Nearest, NearestEven, Away };
enum class OverflowMode { Error, Wrap, Saturate };
enum class OutputMode { Full, Value, Raw, Hex, Bin };
enum class ColorMode { Auto, Always, Never };

struct Format {
  int bits = 32;
  int frac = 16;
  bool signed_value = true;
};

struct Session {
  int bits = 32;
  int frac = 16;
  bool signed_value = true;
  RoundingMode rounding = RoundingMode::NearestEven;
  OverflowMode overflow = OverflowMode::Error;
  OutputMode output = OutputMode::Value;
  ColorMode color = ColorMode::Auto;
  bool has_ans = false;
  mpz_class ans_raw = 0;
  Format ans_format;
};

std::string to_string(RoundingMode mode);
std::string to_string(OverflowMode mode);
std::string to_string(OutputMode mode);
std::string to_string(ColorMode mode);

bool parse_rounding(const std::string& text, RoundingMode& out);
bool parse_overflow(const std::string& text, OverflowMode& out);
bool parse_output(const std::string& text, OutputMode& out);
bool parse_color(const std::string& text, ColorMode& out);
bool parse_bool(const std::string& text, bool& out);

std::string validate_session(const Session& session);
Format session_format(const Session& session);
int int_bits(const Session& session);
std::string format_name(const Format& format);
std::string settings_line(const Session& session);
std::string statusline(const Session& session, bool color_enabled);
