#!/usr/bin/env bash
set -euo pipefail

FPC="$1"
TMP="$(mktemp)"
trap 'rm -f "$TMP"' EXIT

check() {
  local name="$1"
  local input="$2"
  local want="$3"
  printf '%s\n' "$input" | "$FPC" > "$TMP" || true
  if ! grep -Fq -- "$want" "$TMP"; then
    echo "FAIL $name: expected '$want'" >&2
    cat "$TMP" >&2
    exit 1
  fi
}

if [[ "$($FPC --version)" != "fpc 0.1.0" ]]; then
  echo "FAIL version" >&2
  exit 1
fi
if [[ "$($FPC -v)" != "fpc 0.1.0" ]]; then
  echo "FAIL short version" >&2
  exit 1
fi
"$FPC" --help > "$TMP"
grep -Fq -- 'usage:' "$TMP"
grep -Fq -- 'fpc --version, -v' "$TMP"
grep -Fq -- 'commands as expressions:' "$TMP"
grep -Fq -- '1.25, 1.25f, 1.25q16' "$TMP"
"$FPC" -h > "$TMP"
grep -Fq -- 'fpc -e <expr>' "$TMP"

"$FPC" -e '1.5 + 2.25' > "$TMP"
grep -Fxq "3.75" "$TMP"
check set_query ':set bits?' 'bits=32'
check set_format_q $':set Q16.16\n:set' 'bits=32 int=16 frac=16'
check set_format_iq $':set I16Q16\n:set' 'bits=32 int=16 frac=16'
check set_format_qfrac $':set bits=24\n:set q8\n:set' 'bits=24 int=16 frac=8'
check set_format_upper_qfrac $':set bits=24\n:set Q4\n:set' 'bits=24 int=20 frac=4'
check default_float '1.25' '1.25'
check float_suffix '1.25f' '1.25'
check float_suffix_format '1.25fQ4' '1.25'
check plain_math '1.5 + 2.25' '3.75'
check suffix_q $':set output=full\n1.5q16' 'format=Q16.16'
check suffix_iq $':set output=full\n1.5i16q4' 'format=Q16.4'
check binary 'b1010 + 0x5' '15'
check raw_input 'asraw 0x00018000' '1.5'
check raw_literal 'r98304' '1.5'
check raw_literal_expr 'r11 * 1.234 + 32.44' '32.4402'
check raw_literal_hex 'r0x00018000 + 0.5' '2'
check raw_literal_bin 'rb11000000000000000' '1.5'
check raw_literal_negative 'r-65536' '-1'
check output_raw $':set output=raw\n1.5' '98304'
check output_hex 'hex 1.5' '0x00018000'
check output_bin 'bin 1.5' 'b00000000000000011000000000000000'
check removed_precision ':set precision=3' 'error: unknown setting: precision'
check rounding $':set frac=2 rounding=floor\n1 / 3' '0.25'
check overflow_error $':set bits=8 frac=4 overflow=error\n8' 'error: overflow'
check overflow_saturate $':set bits=8 frac=4 overflow=saturate\n8' '7.9375'
check overflow_wrap $':set bits=8 frac=4 overflow=wrap\n8' '-8'
check shift_left '1 << 4' '16'
check shift_right '16 >> 2' '4'
check shift_negative '1 << -1' 'error: negative shift amount'
check validation ':set bits=16 frac=16 signed=true' 'error: signed format needs at least one integer/sign bit'
check unsigned_valid $':set bits=16 frac=16 signed=false\n:set' 'signed=false'
check invalid_rounding ':set rounding=banana' 'error: invalid rounding'
check invalid_binary 'b102' 'error:'
check parens '(1 + 2) * 3' '9'
check ans_basic $'1 + 2\nans * 3' '9'
check ans_asraw $'asraw 0x00018000\nans + 0.5' '2'
check ans_unset 'ans' 'error: ans is not set'
check range_default ':range' 'raw=[-2147483648, 2147483647]'
check range_default_double ':range' 'double=[-32768, 32767.999984741211]'
check range_uq4 $':set bits=8 frac=4 signed=false\n:range' 'double=[0, 15.9375]'
check range_q4 ':range Q4.4' 'raw=[-128, 127]'
check range_i4q4 ':range I4Q4' 'double=[-8, 7.9375]'
check range_qonly $':set bits=16 frac=8\n:range Q4' 'raw=[-32768, 32767]'
check multiline_help ':help' 'literals:'
check help_range ':help' ':range'
check help_rounding ':help' 'trunc, zero, floor, ceil, nearest, nearest_even, away'
check help_overflow ':help' 'error, wrap, saturate'
check epsilon_default ':eps' '0.0000152587890625'
check epsilon_i1q1 $':set bits=2 frac=1\n:eps' '0.5'
check epsilon_q16_16 ':eps Q16.16' '0.0000152587890625'
check epsilon_i16q16 ':eps I16Q16' '0.0000152587890625'
check epsilon_q4 ':eps q4' '0.0625'
"$FPC" '1.5' > "$TMP"
grep -Fxq '1.5' "$TMP"
check forced_color $':set color=always\n1' $'\033[1m'

for example in basic rounding overflow shifts literals; do
  "$FPC" "${BASH_SOURCE%/*}/examples/${example}.fpc" > "$TMP" || true
  grep -Eq '[0-9]' "$TMP"
done
