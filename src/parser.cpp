#include "parser.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
std::string trim(const std::string& s) {
  auto a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return {};
  auto b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

bool starts(const std::string& s, const std::string& p) { return s.rfind(p, 0) == 0; }

cpp_int parse_uint_base(const std::string& s, int base) {
  cpp_int v = 0;
  for (char c : s) {
    int d = -1;
    if (c >= '0' && c <= '9') d = c - '0';
    else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
    if (d < 0 || d >= base) throw std::runtime_error("invalid digit");
    if (v > (std::numeric_limits<cpp_int>::max() - d) / base) throw std::runtime_error("integer literal too large");
    v = v * base + d;
  }
  return v;
}

bool parse_int_text(const std::string& text, int& out) {
  try {
    size_t pos = 0;
    int v = std::stoi(text, &pos);
    if (pos != text.size()) return false;
    out = v;
    return true;
  } catch (...) { return false; }
}

bool parse_format_suffix(const std::string& suffix, const Session& session, Format& out) {
  std::string s = suffix;
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (s.empty()) { out = session_format(session); return true; }
  if (s[0] == 'q') {
    auto dot = s.find('.');
    if (dot == std::string::npos) {
      int frac = 0;
      if (!parse_int_text(s.substr(1), frac)) return false;
      out = Format{session.bits, frac, session.signed_value};
      return true;
    }
    int ib = 0, frac = 0;
    if (!parse_int_text(s.substr(1, dot - 1), ib) || !parse_int_text(s.substr(dot + 1), frac)) return false;
    out = Format{ib + frac, frac, session.signed_value};
    return true;
  }
  if (s[0] == 'i') {
    auto q = s.find('q');
    if (q == std::string::npos) return false;
    int ib = 0, frac = 0;
    if (!parse_int_text(s.substr(1, q - 1), ib) || !parse_int_text(s.substr(q + 1), frac)) return false;
    out = Format{ib + frac, frac, session.signed_value};
    return true;
  }
  return false;
}

EvalResult make_from_ieee(double value, int mantissa_bits, const Format& format, const Session& session) {
  if (!std::isfinite(value)) return {{}, "invalid floating literal"};
  if (value == 0.0) return make_from_raw(0, format, session);
  const bool neg = value < 0.0;
  double abs_value = neg ? -value : value;
  int exp = 0;
  double mantissa = std::frexp(abs_value, &exp);
  cpp_int numerator = static_cast<unsigned long>(std::ldexp(mantissa, mantissa_bits));
  int denom_exp = mantissa_bits - exp;
  cpp_int denominator = 1;
  if (denom_exp >= 0) denominator = pow2(denom_exp);
  else numerator <<= -denom_exp;
  if (neg) numerator = -numerator;
  return make_from_rational(numerator, denominator, format, session);
}

class Parser {
public:
  Parser(std::string text, const Session& session) : text_(std::move(text)), session_(session) {}
  EvalResult parse() {
    auto r = parse_shift();
    if (!r.error.empty()) return r;
    skip();
    if (pos_ != text_.size()) return {{}, "unexpected input"};
    return r;
  }
private:
  EvalResult parse_shift() {
    auto left = parse_add();
    while (true) {
      skip();
      if (match("<<")) { auto right = parse_add(); if (!right.error.empty()) return right; left = shl_fixed(left.value, right.value, session_); }
      else if (match(">>")) { auto right = parse_add(); if (!right.error.empty()) return right; left = shr_fixed(left.value, right.value, session_); }
      else break;
      if (!left.error.empty()) return left;
    }
    return left;
  }

  EvalResult parse_add() {
    auto left = parse_mul();
    while (true) {
      skip();
      if (match("+")) { auto right = parse_mul(); if (!right.error.empty()) return right; left = add_fixed(left.value, right.value, session_); }
      else if (match("-")) { auto right = parse_mul(); if (!right.error.empty()) return right; left = sub_fixed(left.value, right.value, session_); }
      else break;
      if (!left.error.empty()) return left;
    }
    return left;
  }

  EvalResult parse_mul() {
    auto left = parse_unary();
    while (true) {
      skip();
      if (match("*")) { auto right = parse_unary(); if (!right.error.empty()) return right; left = mul_fixed(left.value, right.value, session_); }
      else if (match("/")) { auto right = parse_unary(); if (!right.error.empty()) return right; left = div_fixed(left.value, right.value, session_); }
      else break;
      if (!left.error.empty()) return left;
    }
    return left;
  }

  EvalResult parse_unary() {
    skip();
    if (match("+")) return parse_unary();
    if (match("-")) {
      auto v = parse_unary();
      if (!v.error.empty()) return v;
      return make_from_raw(-v.value.raw, v.value.format, session_);
    }
    return parse_primary();
  }

  EvalResult parse_primary() {
    skip();
    if (match("(")) {
      auto v = parse_shift();
      skip();
      if (!match(")")) return {{}, "missing ')'"};
      return v;
    }
    if (match_identifier("ans")) {
      if (!session_.has_ans) return {{}, "ans is not set"};
      return {Fixed{session_.ans_raw, session_.ans_format}, {}};
    }
    return parse_number();
  }

  EvalResult parse_number() {
    skip();
    const size_t start = pos_;
    if (pos_ < text_.size() && (text_[pos_] == 'r' || text_[pos_] == 'R')) {
      ++pos_;
      bool neg = false;
      if (pos_ < text_.size() && text_[pos_] == '-') {
        neg = true;
        ++pos_;
      }
      int base = 10;
      if (starts(text_.substr(pos_), "0x")) {
        base = 16;
        pos_ += 2;
      } else if (pos_ < text_.size() && text_[pos_] == 'b') {
        base = 2;
        ++pos_;
      }
      size_t ds = pos_;
      if (base == 16) {
        while (pos_ < text_.size() && std::isxdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
      } else if (base == 2) {
        while (pos_ < text_.size() && (text_[pos_] == '0' || text_[pos_] == '1')) ++pos_;
        if (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) return {{}, "invalid raw binary literal"};
      } else {
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
      }
      if (ds == pos_) return {{}, "invalid raw literal"};
      cpp_int raw = parse_uint_base(text_.substr(ds, pos_ - ds), base);
      if (neg) raw = -raw;
      return make_from_raw(raw, session_format(session_), session_);
    }
    if (starts(text_.substr(pos_), "0x")) {
      pos_ += 2;
      size_t ds = pos_;
      while (pos_ < text_.size() && std::isxdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
      if (ds == pos_) return {{}, "invalid hex literal"};
      std::string digits = text_.substr(ds, pos_ - ds);
      std::string suffix = read_suffix();
      Format f;
      if (!parse_format_suffix(suffix, session_, f)) return {{}, "invalid format suffix"};
      return make_from_rational(parse_uint_base(digits, 16), 1, f, session_);
    }
    if (pos_ < text_.size() && text_[pos_] == 'b') {
      ++pos_;
      size_t ds = pos_;
      while (pos_ < text_.size() && (text_[pos_] == '0' || text_[pos_] == '1')) ++pos_;
      if (ds == pos_) return {{}, "invalid binary literal"};
      std::string digits = text_.substr(ds, pos_ - ds);
      if (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) return {{}, "invalid binary literal"};
      std::string suffix = read_suffix();
      Format f;
      if (!parse_format_suffix(suffix, session_, f)) return {{}, "invalid format suffix"};
      return make_from_rational(parse_uint_base(digits, 2), 1, f, session_);
    }
    bool saw_digit = false;
    while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) { ++pos_; saw_digit = true; }
    bool saw_dot = false;
    if (pos_ < text_.size() && text_[pos_] == '.') {
      saw_dot = true; ++pos_;
      while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) { ++pos_; saw_digit = true; }
    }
    if (!saw_digit) return {{}, "expected number"};
    std::string num = text_.substr(start, pos_ - start);
    bool float_suffix = false;
    if (pos_ < text_.size() && (text_[pos_] == 'f' || text_[pos_] == 'F')) {
      float_suffix = true;
      ++pos_;
    }
    std::string suffix = read_suffix();
    Format f;
    if (!parse_format_suffix(suffix, session_, f)) return {{}, "invalid format suffix"};
    if (saw_dot || float_suffix) {
      char* end = nullptr;
      if (float_suffix) {
        float v = std::strtof(num.c_str(), &end);
        if (!end || *end != '\0') return {{}, "invalid floating literal"};
        return make_from_ieee(v, std::numeric_limits<float>::digits, f, session_);
      }
      double v = std::strtod(num.c_str(), &end);
      if (!end || *end != '\0') return {{}, "invalid floating literal"};
      return make_from_ieee(v, std::numeric_limits<double>::digits, f, session_);
    }
    auto dot = num.find('.');
    if (dot == std::string::npos) return make_from_rational(parse_uint_base(num, 10), 1, f, session_);
    std::string a = num.substr(0, dot);
    std::string b = num.substr(dot + 1);
    cpp_int den = 1;
    for (size_t i = 0; i < b.size(); ++i) den *= 10;
    cpp_int n = (a.empty() ? 0 : parse_uint_base(a, 10)) * den + (b.empty() ? 0 : parse_uint_base(b, 10));
    return make_from_rational(n, den, f, session_);
  }

  std::string read_suffix() {
    size_t s = pos_;
    if (pos_ < text_.size() && (text_[pos_] == 'q' || text_[pos_] == 'Q' || text_[pos_] == 'i' || text_[pos_] == 'I')) {
      ++pos_;
      while (pos_ < text_.size() && (std::isalnum(static_cast<unsigned char>(text_[pos_])) || text_[pos_] == '.')) ++pos_;
    }
    return text_.substr(s, pos_ - s);
  }

  void skip() { while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) ++pos_; }
  bool match(const std::string& s) {
    skip();
    if (text_.substr(pos_, s.size()) == s) { pos_ += s.size(); return true; }
    return false;
  }

  bool match_identifier(const std::string& s) {
    skip();
    if (text_.substr(pos_, s.size()) != s) return false;
    size_t end = pos_ + s.size();
    if (end < text_.size() && (std::isalnum(static_cast<unsigned char>(text_[end])) || text_[end] == '_')) return false;
    pos_ = end;
    return true;
  }

  std::string text_;
  const Session& session_;
  size_t pos_ = 0;
};

LineResult command_set(const std::string& args, Session& session) {
  std::string t = trim(args);
  if (t.empty()) return {false, true, false, settings_line(session)};
  if (t.back() == '?' && t.find(' ') == std::string::npos) {
    std::string key = t.substr(0, t.size() - 1);
    if (key == "bits") return {false, true, false, "bits=" + std::to_string(session.bits)};
    if (key == "frac") return {false, true, false, "frac=" + std::to_string(session.frac)};
    if (key == "signed") return {false, true, false, std::string("signed=") + (session.signed_value ? "true" : "false")};
    if (key == "rounding") return {false, true, false, "rounding=" + to_string(session.rounding)};
    if (key == "overflow") return {false, true, false, "overflow=" + to_string(session.overflow)};
    if (key == "output") return {false, true, false, "output=" + to_string(session.output)};
    if (key == "color") return {false, true, false, "color=" + to_string(session.color)};
    return {false, true, true, "unknown setting: " + key};
  }
  Session next = session;
  std::istringstream is(t);
  std::string item;
  while (is >> item) {
    auto eq = item.find('=');
    if (eq == std::string::npos) {
      Format f;
      if (!parse_format_suffix(item, next, f)) return {false, true, true, "expected name=value or format"};
      next.bits = f.bits;
      next.frac = f.frac;
      continue;
    }
    std::string key = item.substr(0, eq), val = item.substr(eq + 1);
    int iv = 0;
    if (key == "bits") { if (!parse_int_text(val, iv)) return {false, true, true, "invalid bits"}; next.bits = iv; }
    else if (key == "frac" || key == "q") { if (!parse_int_text(val, iv)) return {false, true, true, "invalid frac"}; next.frac = iv; }
    else if (key == "signed") { if (!parse_bool(val, next.signed_value)) return {false, true, true, "invalid signed"}; }
    else if (key == "rounding") { if (!parse_rounding(val, next.rounding)) return {false, true, true, "invalid rounding"}; }
    else if (key == "overflow") { if (!parse_overflow(val, next.overflow)) return {false, true, true, "invalid overflow"}; }
    else if (key == "output") { if (!parse_output(val, next.output)) return {false, true, true, "invalid output"}; }
    else if (key == "color") { if (!parse_color(val, next.color)) return {false, true, true, "invalid color"}; }
    else if (key == "format") {
      Format f;
      if (!parse_format_suffix(val, next, f)) return {false, true, true, "invalid format"};
      next.bits = f.bits; next.frac = f.frac;
    } else return {false, true, true, "unknown setting: " + key};
  }
  std::string err = validate_session(next);
  if (!err.empty()) return {false, true, true, err};
  session = next;
  return {false, false, false, {}};
}
}

EvalResult eval_expression(const std::string& text, const Session& session) { return Parser(text, session).parse(); }

std::string help_text() {
  return
    "commands:\n"
    "  :set                         print all settings\n"
    "  :set name?                   print one setting\n"
    "  :set name=value [...]        change settings\n"
    "  :eps [Q16.16|I16Q16|q16]     print fixed-point epsilon\n"
    "  :range [Q16.16|I16Q16|q16]   print raw and double min/max\n"
    "  :q, :quit                    exit\n"
    "settings:\n"
    "  bits, frac, signed, rounding, overflow, output, color\n"
    "  :set Q16.16, :set I16Q16, :set q16\n"
    "rounding options:\n"
    "  trunc, zero, floor, ceil, nearest, nearest_even, away\n"
    "overflow options:\n"
    "  error, wrap, saturate\n"
    "output options:\n"
    "  value, full, raw, hex, bin\n"
    "color options:\n"
    "  auto, always, never\n"
    "expressions:\n"
    "  + - * / << >>, parentheses, ans for the last result\n"
    "literals:\n"
    "  1.25, 1.25f, 1.25q16, 1.25q12.4, 1.25i16q4, b1010, 0x10, r11\n"
    "commands as expressions:\n"
    "  asraw 0x00010000, raw 1.5, hex 1.5, bin 1.5, float 1.5, double 1.5";
}

namespace {
void store_ans(Session& session, const Fixed& value) {
  session.has_ans = true;
  session.ans_raw = value.raw;
  session.ans_format = value.format;
}

std::string range_line(const Format& format) {
  cpp_int min_raw = format.signed_value ? -pow2(format.bits - 1) : cpp_int(0);
  cpp_int max_raw = format.signed_value ? pow2(format.bits - 1) - 1 : pow2(format.bits) - 1;
  Fixed min_value{min_raw, format};
  Fixed max_value{max_raw, format};
  std::ostringstream os;
  os << "raw=[" << raw_decimal(min_value) << ", " << raw_decimal(max_value) << "]\n"
     << "double=[" << double_value(min_value) << ", " << double_value(max_value) << "]";
  return os.str();
}
}

LineResult execute_line(const std::string& line, Session& session) {
  std::string t = trim(line);
  if (t.empty() || starts(t, "#")) return {};
    if (t[0] == ':') {
    std::string cmd = trim(t.substr(1));
    if (cmd == "q" || cmd == "quit") return {true, false, false, {}};
    if (cmd == "range" || starts(cmd, "range ")) {
      Format format = session_format(session);
      std::string arg = trim(cmd.size() == 5 ? "" : cmd.substr(5));
      if (!arg.empty() && !parse_format_suffix(arg, session, format)) return {false, true, true, "invalid format"};
      return {false, true, false, range_line(format)};
    }
    if (cmd == "eps" || starts(cmd, "eps ")) {
      Format format = session_format(session);
      std::string arg = trim(cmd.size() == 3 ? "" : cmd.substr(3));
      if (!arg.empty() && !parse_format_suffix(arg, session, format)) return {false, true, true, "invalid format"};
      auto r = make_from_raw(1, format, session);
      if (!r.error.empty()) return {false, true, true, r.error};
      return {false, true, false, decimal_value(r.value)};
    }
    if (cmd == "h" || cmd == "help") return {false, true, false, help_text()};
    if (starts(cmd, "set")) return command_set(cmd.size() == 3 ? "" : cmd.substr(3), session);
    return {false, true, true, "unknown command"};
  }
  auto output_expr = [&](const std::string& prefix, OutputMode mode) -> LineResult {
    auto r = eval_expression(trim(t.substr(prefix.size())), session);
    if (!r.error.empty()) return {false, true, true, r.error};
    store_ans(session, r.value);
    if (mode == OutputMode::Raw) return {false, true, false, raw_decimal(r.value)};
    if (mode == OutputMode::Hex) return {false, true, false, raw_hex(r.value)};
    if (mode == OutputMode::Bin) return {false, true, false, raw_bin(r.value)};
    if (prefix == "float ") return {false, true, false, float_value(r.value)};
    if (prefix == "double ") return {false, true, false, double_value(r.value)};
    return {false, true, false, decimal_value(r.value)};
  };
  if (starts(t, "raw ")) return output_expr("raw ", OutputMode::Raw);
  if (starts(t, "hex ")) return output_expr("hex ", OutputMode::Hex);
  if (starts(t, "bin ")) return output_expr("bin ", OutputMode::Bin);
  if (starts(t, "float ")) return output_expr("float ", OutputMode::Value);
  if (starts(t, "double ")) return output_expr("double ", OutputMode::Value);
  if (starts(t, "asraw ")) {
    std::string raw = trim(t.substr(6));
    cpp_int v;
    try {
      bool neg = starts(raw, "-");
      if (neg) raw = raw.substr(1);
      if (starts(raw, "0x")) v = parse_uint_base(raw.substr(2), 16);
      else if (starts(raw, "b")) v = parse_uint_base(raw.substr(1), 2);
      else v = parse_uint_base(raw, 10);
      if (neg) v = -v;
    } catch (...) { return {false, true, true, "invalid raw integer"}; }
    auto r = make_from_raw(v, session_format(session), session);
    if (!r.error.empty()) return {false, true, true, r.error};
    store_ans(session, r.value);
    return {false, true, false, format_result(r.value, session)};
  }
  auto r = eval_expression(t, session);
  if (!r.error.empty()) return {false, true, true, r.error};
  store_ans(session, r.value);
  return {false, true, false, format_result(r.value, session)};
}
