#pragma once

#include "session.hpp"

#include <gmpxx.h>
#include <string>

using cpp_int = mpz_class;

struct Fixed {
  cpp_int raw = 0;
  Format format;
};

struct EvalResult {
  Fixed value;
  std::string error;
};

cpp_int pow2(int n);
cpp_int div_round(cpp_int numerator, cpp_int denominator, RoundingMode mode);
EvalResult make_from_rational(cpp_int numerator, cpp_int denominator, const Format& format, const Session& session);
EvalResult make_from_raw(cpp_int raw, const Format& format, const Session& session);
EvalResult convert_format(const Fixed& value, const Format& format, const Session& session);
EvalResult add_fixed(const Fixed& a, const Fixed& b, const Session& session);
EvalResult sub_fixed(const Fixed& a, const Fixed& b, const Session& session);
EvalResult mul_fixed(const Fixed& a, const Fixed& b, const Session& session);
EvalResult div_fixed(const Fixed& a, const Fixed& b, const Session& session);
EvalResult shl_fixed(const Fixed& a, const Fixed& b, const Session& session);
EvalResult shr_fixed(const Fixed& a, const Fixed& b, const Session& session);

std::string decimal_value(const Fixed& value);
std::string float_value(const Fixed& value);
std::string double_value(const Fixed& value);
std::string raw_decimal(const Fixed& value);
std::string raw_hex(const Fixed& value, int min_bits = 0);
std::string raw_bin(const Fixed& value, int min_bits = 0);
std::string format_result(const Fixed& value, const Session& session);
bool integer_shift_amount(const Fixed& value, long long& out);
