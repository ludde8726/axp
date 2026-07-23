"""Every documented error path: division by zero, domain errors (0^0, ln of
a non-positive number, negative base with a non-integer exponent), and
malformed-input parse errors.
"""
from ctypes import byref

import framework
from axp_bindings import (
  AXP_Int, AXP_Float, new_ctx, AXP_ERR_DIV_ZERO, AXP_ERR_PARSE,
  axp_addi, axp_divi, axp_powi, axp_divf, axp_powf, axp_lnf, axp_powff,
  axp_atoi, axp_atof, axp_freei, axp_freef, err_name, err_str,
  int_to_axpi, str_to_axpf,
)

ctx = new_ctx(precision=16)

def _expect_div_zero(s, ok, label):
  s.check_raises(ok, ctx, AXP_ERR_DIV_ZERO, label, err_name, err_str)

def _expect_parse_err(s, ok, label):
  s.check_raises(ok, ctx, AXP_ERR_PARSE, label, err_name, err_str)

def run(report):
  with framework.Suite("errors", report) as s:
    # integer division by zero
    x, y, r, rem = int_to_axpi(ctx, 10), int_to_axpi(ctx, 0), AXP_Int(), AXP_Int()
    ok = axp_divi(byref(ctx), byref(x), byref(y), byref(r), byref(rem))
    _expect_div_zero(s, ok, "axp_divi(10, 0) fails with AXP_ERR_DIV_ZERO")
    axp_freei(byref(x)); axp_freei(byref(y))

    # integer 0^0
    x, r = int_to_axpi(ctx, 0), AXP_Int()
    ok = axp_powi(byref(ctx), byref(x), 0, byref(r))
    _expect_div_zero(s, ok, "axp_powi(0, 0) fails with AXP_ERR_DIV_ZERO")
    axp_freei(byref(x))

    # float division by zero
    x, y, r = str_to_axpf(ctx, "10.0"), str_to_axpf(ctx, "0.0"), AXP_Float()
    ok = axp_divf(byref(ctx), byref(x), byref(y), byref(r))
    _expect_div_zero(s, ok, "axp_divf(10.0, 0.0) fails with AXP_ERR_DIV_ZERO")
    axp_freef(byref(x)); axp_freef(byref(y))

    # float 0^0 and 0^negative
    x, r = str_to_axpf(ctx, "0.0"), AXP_Float()
    ok = axp_powf(byref(ctx), byref(x), 0, byref(r))
    _expect_div_zero(s, ok, "axp_powf(0.0, 0) fails with AXP_ERR_DIV_ZERO")
    axp_freef(byref(x))

    x, r = str_to_axpf(ctx, "0.0"), AXP_Float()
    ok = axp_powf(byref(ctx), byref(x), -3, byref(r))
    _expect_div_zero(s, ok, "axp_powf(0.0, -3) fails with AXP_ERR_DIV_ZERO")
    axp_freef(byref(x))

    # ln domain errors
    x, r = str_to_axpf(ctx, "0.0"), AXP_Float()
    ok = axp_lnf(byref(ctx), byref(x), byref(r))
    _expect_div_zero(s, ok, "axp_lnf(0.0) fails with AXP_ERR_DIV_ZERO")
    axp_freef(byref(x))

    x, r = str_to_axpf(ctx, "-5.0"), AXP_Float()
    ok = axp_lnf(byref(ctx), byref(x), byref(r))
    _expect_div_zero(s, ok, "axp_lnf(-5.0) fails with AXP_ERR_DIV_ZERO")
    axp_freef(byref(x))

    # powff domain errors
    x, y, r = str_to_axpf(ctx, "0.0"), str_to_axpf(ctx, "0.0"), AXP_Float()
    ok = axp_powff(byref(ctx), byref(x), byref(y), byref(r))
    _expect_div_zero(s, ok, "axp_powff(0.0, 0.0) fails with AXP_ERR_DIV_ZERO")
    axp_freef(byref(x)); axp_freef(byref(y))

    x, y, r = str_to_axpf(ctx, "0.0"), str_to_axpf(ctx, "-2.5"), AXP_Float()
    ok = axp_powff(byref(ctx), byref(x), byref(y), byref(r))
    _expect_div_zero(s, ok, "axp_powff(0.0, -2.5) fails with AXP_ERR_DIV_ZERO")
    axp_freef(byref(x)); axp_freef(byref(y))

    x, y, r = str_to_axpf(ctx, "-2.0"), str_to_axpf(ctx, "3.5"), AXP_Float()
    ok = axp_powff(byref(ctx), byref(x), byref(y), byref(r))
    _expect_div_zero(s, ok, "axp_powff(-2.0, 3.5) [negative base, non-integer exponent] fails")
    axp_freef(byref(x)); axp_freef(byref(y))

    # powff with a negative base and an integer exponent must still succeed
    x, y, r = str_to_axpf(ctx, "-2.0"), str_to_axpf(ctx, "3.0"), AXP_Float()
    ok = axp_powff(byref(ctx), byref(x), byref(y), byref(r))
    s.check(ok, "axp_powff(-2.0, 3.0) [negative base, integer exponent] still succeeds")
    axp_freef(byref(x)); axp_freef(byref(y))
    if ok: axp_freef(byref(r))

    # atoi parse errors
    x = AXP_Int()
    ok = axp_atoi(byref(ctx), b"", byref(x))
    _expect_parse_err(s, ok, "atoi('') fails with AXP_ERR_PARSE")

    x = AXP_Int()
    ok = axp_atoi(byref(ctx), b"12a34", byref(x))
    _expect_parse_err(s, ok, "atoi('12a34') fails with AXP_ERR_PARSE")

    x = AXP_Int()
    ok = axp_atoi(byref(ctx), b"--5", byref(x))
    _expect_parse_err(s, ok, "atoi('--5') fails with AXP_ERR_PARSE")

    # atof parse errors
    x = AXP_Float()
    ok = axp_atof(byref(ctx), b"", byref(x))
    _expect_parse_err(s, ok, "atof('') fails with AXP_ERR_PARSE")

    x = AXP_Float()
    ok = axp_atof(byref(ctx), b"abc", byref(x))
    _expect_parse_err(s, ok, "atof('abc') fails with AXP_ERR_PARSE")

    x = AXP_Float()
    ok = axp_atof(byref(ctx), b"1.2.3", byref(x))
    _expect_parse_err(s, ok, "atof('1.2.3') [multiple decimal points] fails with AXP_ERR_PARSE")

    x = AXP_Float()
    ok = axp_atof(byref(ctx), b"1e", byref(x))
    _expect_parse_err(s, ok, "atof('1e') [missing exponent digits] fails with AXP_ERR_PARSE")

    x = AXP_Float()
    ok = axp_atof(byref(ctx), b"e5", byref(x))
    _expect_parse_err(s, ok, "atof('e5') [no mantissa digits] fails with AXP_ERR_PARSE")

    x = AXP_Float()
    ok = axp_atof(byref(ctx), b"-", byref(x))
    _expect_parse_err(s, ok, "atof('-') [sign only] fails with AXP_ERR_PARSE")

    # After every failed atof/atoi above, the output struct should be untouched/uninitialized 
    # Confirm parsing "0" right afterwards still works cleanly
    x = AXP_Float()
    ok = axp_atof(byref(ctx), b"0", byref(x))
    s.check(ok, "atof('0') succeeds (regression check for the fixed use-after-free)")
    if ok:
      r = AXP_Float()
      ln_ok = axp_lnf(byref(ctx), byref(x), byref(r))
      s.check(not ln_ok, "ln(atof('0')) cleanly fails rather than crashing")
      axp_freef(byref(x))
