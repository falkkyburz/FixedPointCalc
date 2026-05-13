#include "fixed.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <cmath>
#include <sstream>
#include <string>

cpp_int pow2(int n) { return cpp_int(1) << n; }

namespace {
std::string validate_format(const Format& f) {
  if (f.bits <= 0) return "bits must be > 0";
  if (f.bits > 64) return "bits must be <= 64";
  if (f.frac < 0) return "frac must be >= 0";
  if (f.frac > f.bits) return "frac must be <= bits";
  if (f.signed_value && f.bits <= f.frac) return "signed format needs at least one integer/sign bit";
  return {};
}

cpp_int min_raw(const Format& f) { return f.signed_value ? -(pow2(f.bits - 1)) : cpp_int(0); }
cpp_int max_raw(const Format& f) { return f.signed_value ? pow2(f.bits - 1) - 1 : pow2(f.bits) - 1; }

cpp_int floor_div(cpp_int n, cpp_int d) {
  cpp_int q = n / d;
  cpp_int r = n % d;
  if (r != 0 && ((r > 0) != (d > 0))) --q;
  return q;
}

cpp_int ceil_div(cpp_int n, cpp_int d) { return -floor_div(-n, d); }

cpp_int abs_int(cpp_int v) { return v < 0 ? -v : v; }

std::string int_to_string(cpp_int v) {
  if (v == 0) return "0";
  const bool neg = v < 0;
  unsigned __int128 n = neg ? static_cast<unsigned __int128>(-v) : static_cast<unsigned __int128>(v);
  std::string out;
  while (n != 0) {
    out.push_back(static_cast<char>('0' + (n % 10)));
    n /= 10;
  }
  if (neg) out.push_back('-');
  std::reverse(out.begin(), out.end());
  return out;
}

double int_to_double(cpp_int v) {
  const bool neg = v < 0;
  unsigned __int128 n = neg ? static_cast<unsigned __int128>(-v) : static_cast<unsigned __int128>(v);
  double out = 0.0;
  double scale = 1.0;
  while (n != 0) {
    if ((n & 1) != 0) out += scale;
    scale *= 2.0;
    n >>= 1;
  }
  return neg ? -out : out;
}

bool mul_overflows(cpp_int a, cpp_int b, cpp_int& out) {
  if (a == 0 || b == 0) { out = 0; return false; }
  const cpp_int max = std::numeric_limits<cpp_int>::max();
  const cpp_int min = std::numeric_limits<cpp_int>::min();
  if (a > 0) {
    if (b > 0) { if (a > max / b) return true; }
    else { if (b < min / a) return true; }
  } else {
    if (b > 0) { if (a < min / b) return true; }
    else { if (a != 0 && b < max / a) return true; }
  }
  out = a * b;
  return false;
}

EvalResult enforce(cpp_int raw, const Format& f, const Session& session) {
  const auto format_error = validate_format(f);
  if (!format_error.empty()) return {{}, format_error};
  const cpp_int lo = min_raw(f);
  const cpp_int hi = max_raw(f);
  if (raw >= lo && raw <= hi) return {Fixed{raw, f}, {}};
  if (session.overflow == OverflowMode::Error) return {{}, "overflow"};
  if (session.overflow == OverflowMode::Saturate) return {Fixed{raw < lo ? lo : hi, f}, {}};
  const cpp_int mod = pow2(f.bits);
  cpp_int wrapped = raw % mod;
  if (wrapped < 0) wrapped += mod;
  if (f.signed_value && wrapped >= pow2(f.bits - 1)) wrapped -= mod;
  return {Fixed{wrapped, f}, {}};
}
}

cpp_int div_round(cpp_int numerator, cpp_int denominator, RoundingMode mode) {
  if (denominator < 0) {
    numerator = -numerator;
    denominator = -denominator;
  }
  switch (mode) {
    case RoundingMode::Trunc:
    case RoundingMode::Zero:
      return numerator / denominator;
    case RoundingMode::Floor:
      return floor_div(numerator, denominator);
    case RoundingMode::Ceil:
      return ceil_div(numerator, denominator);
    case RoundingMode::Away: {
      cpp_int q = numerator / denominator;
      cpp_int r = numerator % denominator;
      if (r != 0) q += numerator >= 0 ? 1 : -1;
      return q;
    }
    case RoundingMode::Nearest:
    case RoundingMode::NearestEven: {
      cpp_int q = numerator / denominator;
      cpp_int r = abs_int(numerator % denominator);
      cpp_int twice = r * 2;
      cpp_int sign = numerator >= 0 ? 1 : -1;
      if (twice > denominator) return q + sign;
      if (twice < denominator) return q;
      if (mode == RoundingMode::Nearest) return q + sign;
      return (q % 2 == 0) ? q : q + sign;
    }
  }
  return numerator / denominator;
}

EvalResult make_from_rational(cpp_int numerator, cpp_int denominator, const Format& format, const Session& session) {
  const auto format_error = validate_format(format);
  if (!format_error.empty()) return {{}, format_error};
  if (denominator == 0) return {{}, "division by zero"};
  cpp_int scaled = 0;
  if (mul_overflows(numerator, pow2(format.frac), scaled)) return {{}, "overflow"};
  cpp_int raw = div_round(scaled, denominator, session.rounding);
  return enforce(raw, format, session);
}

EvalResult make_from_raw(cpp_int raw, const Format& format, const Session& session) { return enforce(raw, format, session); }

EvalResult convert_format(const Fixed& value, const Format& format, const Session& session) {
  cpp_int raw = value.raw;
  const int diff = format.frac - value.format.frac;
  if (diff > 0) raw <<= diff;
  else if (diff < 0) raw = div_round(raw, pow2(-diff), session.rounding);
  return enforce(raw, format, session);
}

EvalResult add_fixed(const Fixed& a, const Fixed& b, const Session& session) {
  auto ca = convert_format(a, session_format(session), session); if (!ca.error.empty()) return ca;
  auto cb = convert_format(b, session_format(session), session); if (!cb.error.empty()) return cb;
  return enforce(ca.value.raw + cb.value.raw, session_format(session), session);
}

EvalResult sub_fixed(const Fixed& a, const Fixed& b, const Session& session) {
  auto ca = convert_format(a, session_format(session), session); if (!ca.error.empty()) return ca;
  auto cb = convert_format(b, session_format(session), session); if (!cb.error.empty()) return cb;
  return enforce(ca.value.raw - cb.value.raw, session_format(session), session);
}

EvalResult mul_fixed(const Fixed& a, const Fixed& b, const Session& session) {
  auto ca = convert_format(a, session_format(session), session); if (!ca.error.empty()) return ca;
  auto cb = convert_format(b, session_format(session), session); if (!cb.error.empty()) return cb;
  cpp_int product = 0;
  if (mul_overflows(ca.value.raw, cb.value.raw, product)) return {{}, "overflow"};
  cpp_int raw = div_round(product, pow2(session.frac), session.rounding);
  return enforce(raw, session_format(session), session);
}

EvalResult div_fixed(const Fixed& a, const Fixed& b, const Session& session) {
  auto ca = convert_format(a, session_format(session), session); if (!ca.error.empty()) return ca;
  auto cb = convert_format(b, session_format(session), session); if (!cb.error.empty()) return cb;
  if (cb.value.raw == 0) return {{}, "division by zero"};
  cpp_int numerator = 0;
  if (mul_overflows(ca.value.raw, pow2(session.frac), numerator)) return {{}, "overflow"};
  cpp_int raw = div_round(numerator, cb.value.raw, session.rounding);
  return enforce(raw, session_format(session), session);
}

bool integer_shift_amount(const Fixed& value, long long& out) {
  if (value.format.frac > 0 && (value.raw % pow2(value.format.frac)) != 0) return false;
  cpp_int n = value.raw >> value.format.frac;
  if (n < cpp_int(-1000001) || n > cpp_int(1000001)) return false;
  out = static_cast<long long>(n);
  return true;
}

EvalResult shl_fixed(const Fixed& a, const Fixed& b, const Session& session) {
  auto ca = convert_format(a, session_format(session), session); if (!ca.error.empty()) return ca;
  long long n = 0;
  if (!integer_shift_amount(b, n)) return {{}, "shift amount must be an integer"};
  if (n < 0) return {{}, "negative shift amount"};
  if (n > 1000000) return {{}, "shift amount too large"};
  return enforce(ca.value.raw << n, session_format(session), session);
}

EvalResult shr_fixed(const Fixed& a, const Fixed& b, const Session& session) {
  auto ca = convert_format(a, session_format(session), session); if (!ca.error.empty()) return ca;
  long long n = 0;
  if (!integer_shift_amount(b, n)) return {{}, "shift amount must be an integer"};
  if (n < 0) return {{}, "negative shift amount"};
  if (n > 1000000) return {{}, "shift amount too large"};
  return enforce(ca.value.raw >> n, session_format(session), session);
}

std::string decimal_value(const Fixed& value) {
  cpp_int scale = pow2(value.format.frac);
  cpp_int ip = value.raw / scale;
  cpp_int rem = value.raw % scale;
  if (rem < 0) rem = -rem;
  std::ostringstream os;
  if (value.raw < 0 && ip == 0) os << '-';
  os << int_to_string(ip);
  if (rem != 0) {
    os << '.';
    for (int i = 0; i < value.format.frac && rem != 0; ++i) {
      rem *= 10;
      cpp_int digit = rem / scale;
      os << static_cast<unsigned>(digit);
      rem %= scale;
    }
  }
  return os.str();
}

namespace {
double to_double(const Fixed& value) {
  return std::ldexp(int_to_double(value.raw), -value.format.frac);
}
}

std::string float_value(const Fixed& value) {
  std::ostringstream os;
  os << std::setprecision(std::numeric_limits<float>::max_digits10) << static_cast<float>(to_double(value));
  return os.str();
}

std::string double_value(const Fixed& value) {
  std::ostringstream os;
  os << std::setprecision(std::numeric_limits<double>::max_digits10) << to_double(value);
  return os.str();
}

std::string raw_decimal(const Fixed& value) { return int_to_string(value.raw); }

std::string raw_hex(const Fixed& value, int min_bits) {
  cpp_int v = value.raw;
  int bits = std::max(min_bits, value.format.bits);
  if (v < 0) v += pow2(bits);
  if (v < 0) v = 0;
  std::string out;
  do {
    cpp_int nibble = v & 0xf;
    int d = static_cast<int>(nibble);
    out.push_back("0123456789abcdef"[d]);
    v >>= 4;
  } while (v > 0);
  while (static_cast<int>(out.size()) < (bits + 3) / 4) out.push_back('0');
  std::reverse(out.begin(), out.end());
  return "0x" + out;
}

std::string raw_bin(const Fixed& value, int min_bits) {
  int bits = std::max(min_bits, value.format.bits);
  cpp_int v = value.raw;
  if (v < 0) v += pow2(bits);
  std::string out = "b";
  for (int i = bits - 1; i >= 0; --i) out.push_back(((v >> i) & 1) != 0 ? '1' : '0');
  return out;
}

std::string format_result(const Fixed& value, const Session& session) {
  switch (session.output) {
    case OutputMode::Value: return decimal_value(value);
    case OutputMode::Raw: return raw_decimal(value);
    case OutputMode::Hex: return raw_hex(value);
    case OutputMode::Bin: return raw_bin(value);
    case OutputMode::Full: {
      std::ostringstream os;
      os << "value=" << decimal_value(value)
         << " raw=" << raw_decimal(value)
         << " hex=" << raw_hex(value)
         << " bin=" << raw_bin(value)
         << " format=" << format_name(value.format);
      return os.str();
    }
  }
  return decimal_value(value);
}
