#include "session.hpp"

#include <algorithm>
#include <sstream>

namespace {
std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

std::string short_rounding(RoundingMode mode) {
  switch (mode) {
    case RoundingMode::Trunc: return "tr";
    case RoundingMode::Zero: return "z";
    case RoundingMode::Floor: return "fl";
    case RoundingMode::Ceil: return "cl";
    case RoundingMode::Nearest: return "n";
    case RoundingMode::NearestEven: return "ne";
    case RoundingMode::Away: return "aw";
  }
  return "?";
}

std::string short_overflow(OverflowMode mode) {
  switch (mode) {
    case OverflowMode::Error: return "err";
    case OverflowMode::Wrap: return "wrp";
    case OverflowMode::Saturate: return "sat";
  }
  return "?";
}
}

std::string to_string(RoundingMode mode) {
  switch (mode) {
    case RoundingMode::Trunc: return "trunc";
    case RoundingMode::Zero: return "zero";
    case RoundingMode::Floor: return "floor";
    case RoundingMode::Ceil: return "ceil";
    case RoundingMode::Nearest: return "nearest";
    case RoundingMode::NearestEven: return "nearest_even";
    case RoundingMode::Away: return "away";
  }
  return "unknown";
}

std::string to_string(OverflowMode mode) {
  switch (mode) {
    case OverflowMode::Error: return "error";
    case OverflowMode::Wrap: return "wrap";
    case OverflowMode::Saturate: return "saturate";
  }
  return "unknown";
}

std::string to_string(OutputMode mode) {
  switch (mode) {
    case OutputMode::Full: return "full";
    case OutputMode::Value: return "value";
    case OutputMode::Raw: return "raw";
    case OutputMode::Hex: return "hex";
    case OutputMode::Bin: return "bin";
  }
  return "unknown";
}

std::string to_string(ColorMode mode) {
  switch (mode) {
    case ColorMode::Auto: return "auto";
    case ColorMode::Always: return "always";
    case ColorMode::Never: return "never";
  }
  return "unknown";
}

bool parse_rounding(const std::string& text, RoundingMode& out) {
  const auto s = lower(text);
  if (s == "trunc") out = RoundingMode::Trunc;
  else if (s == "zero") out = RoundingMode::Zero;
  else if (s == "floor") out = RoundingMode::Floor;
  else if (s == "ceil") out = RoundingMode::Ceil;
  else if (s == "nearest") out = RoundingMode::Nearest;
  else if (s == "nearest_even") out = RoundingMode::NearestEven;
  else if (s == "away") out = RoundingMode::Away;
  else return false;
  return true;
}

bool parse_overflow(const std::string& text, OverflowMode& out) {
  const auto s = lower(text);
  if (s == "error") out = OverflowMode::Error;
  else if (s == "wrap") out = OverflowMode::Wrap;
  else if (s == "saturate") out = OverflowMode::Saturate;
  else return false;
  return true;
}

bool parse_output(const std::string& text, OutputMode& out) {
  const auto s = lower(text);
  if (s == "full") out = OutputMode::Full;
  else if (s == "value") out = OutputMode::Value;
  else if (s == "raw") out = OutputMode::Raw;
  else if (s == "hex") out = OutputMode::Hex;
  else if (s == "bin") out = OutputMode::Bin;
  else return false;
  return true;
}

bool parse_color(const std::string& text, ColorMode& out) {
  const auto s = lower(text);
  if (s == "auto") out = ColorMode::Auto;
  else if (s == "always") out = ColorMode::Always;
  else if (s == "never") out = ColorMode::Never;
  else return false;
  return true;
}

bool parse_bool(const std::string& text, bool& out) {
  const auto s = lower(text);
  if (s == "true" || s == "1" || s == "yes" || s == "on") out = true;
  else if (s == "false" || s == "0" || s == "no" || s == "off") out = false;
  else return false;
  return true;
}

std::string validate_session(const Session& s) {
  if (s.bits <= 0) return "bits must be > 0";
  if (s.frac < 0) return "frac must be >= 0";
  if (s.frac > s.bits) return "frac must be <= bits";
  if (s.signed_value && s.bits <= s.frac) return "signed format needs at least one integer/sign bit";
  return {};
}

Format session_format(const Session& s) { return Format{s.bits, s.frac, s.signed_value}; }
int int_bits(const Session& s) { return s.bits - s.frac; }

std::string format_name(const Format& f) {
  std::ostringstream os;
  os << (f.signed_value ? "Q" : "UQ") << (f.bits - f.frac) << "." << f.frac;
  return os.str();
}

std::string settings_line(const Session& s) {
  std::ostringstream os;
  os << "bits=" << s.bits << " int=" << int_bits(s) << " frac=" << s.frac
     << " signed=" << (s.signed_value ? "true" : "false")
     << " rounding=" << to_string(s.rounding)
     << " overflow=" << to_string(s.overflow)
     << " output=" << to_string(s.output)
     << " color=" << to_string(s.color);
  return os.str();
}

std::string statusline(const Session& s, bool color_enabled) {
  std::ostringstream os;
  if (color_enabled) os << "\033[2;36m";
  os << "[" << (s.signed_value ? "s" : "u") << s.bits << "b"
     << " Q" << int_bits(s) << "." << s.frac
     << " rnd=" << short_rounding(s.rounding)
     << " ovf=" << short_overflow(s.overflow) << "]";
  if (color_enabled) os << "\033[0m\033[1;32m > \033[0m";
  else os << " > ";
  return os.str();
}
