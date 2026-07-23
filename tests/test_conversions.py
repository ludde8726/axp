import random
from ctypes import byref
from decimal import Decimal, getcontext, ROUND_HALF_UP

import framework
from axp_bindings import (
  AXP_Int, AXP_Float, new_ctx, axp_atoi, axp_atof, axp_itoa, axp_ftoa, axp_ftoa_ex,
  AXP_FTOA_REGULAR, AXP_FTOA_SCIENTIFIC, AXP_FTOA_AUTO,
  axp_freei, axp_freef, int_to_axpi, axpi_to_int, str_to_axpf, axpf_to_str, axpf_to_str_ex,
)
from helpers import gen_randomi, gen_randomf

ctx = new_ctx(precision=16)
_REF_GUARD_PREC = 50

def _correctly_rounded(compute, target_prec):
  getcontext().prec = target_prec + _REF_GUARD_PREC
  getcontext().rounding = ROUND_HALF_UP
  v = compute()
  getcontext().prec = target_prec
  getcontext().rounding = ROUND_HALF_UP
  return +v

def run_itoa_roundtrip(x):
  ax = int_to_axpi(ctx, x)
  got = axpi_to_int(ax)
  axp_freei(byref(ax))
  return got, x, f"itoa(atoi({x}))"

def run_ftoa_roundtrip(x_str):
  ax = str_to_axpf(ctx, x_str)
  got_str = axpf_to_str(ctx, ax)
  axp_freef(byref(ax))
  expected = _correctly_rounded(lambda: Decimal(x_str), ctx.precision)
  return Decimal(got_str), expected, f"ftoa(atof({x_str}))"

def run_ftoa_ex_value_roundtrip(x_str, fmt):
  ax = str_to_axpf(ctx, x_str)
  rendered = axpf_to_str_ex(ax, fmt)
  axp_freef(byref(ax))
  rt = str_to_axpf(ctx, rendered)
  got = Decimal(axpf_to_str(ctx, rt))
  axp_freef(byref(rt))
  expected = _correctly_rounded(lambda: Decimal(x_str), ctx.precision)
  return got, expected, f"parse(ftoa_ex(atof({x_str}), fmt={fmt}))"

def run(report):
  with framework.Suite("conversions", report) as s:
    # itoa/atoi edge cases
    s.check_equal(run_itoa_roundtrip(0)[0], 0, "atoi/itoa round-trips 0")
    s.check_equal(run_itoa_roundtrip(-0)[0], 0, "atoi/itoa round-trips -0 as 0")
    s.check_equal(run_itoa_roundtrip(1)[0], 1, "atoi/itoa round-trips 1")
    s.check_equal(run_itoa_roundtrip(-1)[0], -1, "atoi/itoa round-trips -1")
    s.check_equal(run_itoa_roundtrip(10 ** 50)[0], 10 ** 50, "atoi/itoa round-trips a 51-digit number")
    s.check_equal(run_itoa_roundtrip(-(10 ** 50))[0], -(10 ** 50), "atoi/itoa round-trips a large negative number")

    # atoi should tolerate whitespace and a leading '+'.
    ax = AXP_Int()
    ok = axp_atoi(byref(ctx), b"  123  ", byref(ax))
    s.check(ok, "atoi tolerates surrounding whitespace")
    if ok:
      s.check_equal(axpi_to_int(ax), 123, "atoi with surrounding whitespace parses correctly")
      axp_freei(byref(ax))

    ax = AXP_Int()
    ok = axp_atoi(byref(ctx), b"+42", byref(ax))
    s.check(ok, "atoi accepts a leading '+'")
    if ok:
      s.check_equal(axpi_to_int(ax), 42, "atoi('+42') == 42")
      axp_freei(byref(ax))

    # atof/ftoa edge cases, including every zero representation
    for zero_str in ["0", "00", "000", "0.0", "0.00", "00.00", "-0", "+0", "0e10", "0.0e-100"]:
      s.check_equal(run_ftoa_roundtrip(zero_str)[0], Decimal("0"), f"atof({zero_str!r}) parses as exactly zero")

    s.check_equal(run_ftoa_roundtrip("123.456")[0], Decimal("123.456"), "atof/ftoa round-trips a plain decimal")
    s.check_equal(run_ftoa_roundtrip("-42")[0], Decimal("-42"), "atof/ftoa round-trips a negative integer-valued float")
    s.check_equal(run_ftoa_roundtrip("0.05")[0], Decimal("0.05"), "atof/ftoa round-trips a leading-zero-after-dot value")
    s.check_equal(run_ftoa_roundtrip("1e10")[0], Decimal("1e10"), "atof/ftoa round-trips positive scientific notation")
    s.check_equal(run_ftoa_roundtrip("1.5e-3")[0], Decimal("1.5e-3"), "atof/ftoa round-trips negative scientific notation")
    s.check_equal(run_ftoa_roundtrip("1.5E3")[0], Decimal("1.5e3"), "atof accepts uppercase E")
    s.check_equal(run_ftoa_roundtrip("  3.14  ")[0], Decimal("3.14"), "atof tolerates surrounding whitespace")

    # atof at ctx.precision=16 should correctly round inputs with more digits than that.
    got, _, _ = run_ftoa_roundtrip("1.00000000000000005")
    s.check_equal(got, Decimal("1"), "atof correctly rounds excess digits below the halfway point")
    got, _, _ = run_ftoa_roundtrip("1.0000000000000005")
    s.check_equal(got, Decimal("1.000000000000001"), "atof correctly rounds excess digits at a halfway tie")

    # axp_ftoa_ex: REGULAR always fixed-point, even for extreme exponents.
    def regular(x_str):
      ax = str_to_axpf(ctx, x_str)
      out = axpf_to_str_ex(ax, AXP_FTOA_REGULAR)
      axp_freef(byref(ax))
      return out

    s.check_equal(regular("123.456"), "123.456", "REGULAR keeps a normal value fixed-point")
    s.check_equal(regular("1e20"), "100000000000000000000.0", "REGULAR never switches to scientific, even for huge exponents")
    s.check_equal(regular("1e-20"), "0.00000000000000000001", "REGULAR never switches to scientific, even for tiny exponents")
    s.check_equal(regular("0.0"), "0.0", "REGULAR renders zero as 0.0")

    # axp_ftoa_ex: SCIENTIFIC always e-notation, even for 'normal' values.
    def scientific(x_str):
      ax = str_to_axpf(ctx, x_str)
      out = axpf_to_str_ex(ax, AXP_FTOA_SCIENTIFIC)
      axp_freef(byref(ax))
      return out

    s.check_equal(scientific("123.456"), "1.23456e+2", "SCIENTIFIC normalizes the mantissa to d.ddd")
    s.check_equal(scientific("5"), "5.0e+0", "SCIENTIFIC always shows a fractional digit, even for a single-digit mantissa")
    s.check_equal(scientific("-0.001"), "-1.0e-3", "SCIENTIFIC switches to e-notation even for a 'small but normal' value")
    s.check_equal(scientific("0.0"), "0.0", "SCIENTIFIC still renders zero as 0.0 (no meaningful exponent)")

    # axp_ftoa_ex: AUTO picks REGULAR or SCIENTIFIC based on padding-zero count.
    def auto(x_str):
      ax = str_to_axpf(ctx, x_str)
      out = axpf_to_str_ex(ax, AXP_FTOA_AUTO)
      axp_freef(byref(ax))
      return out

    s.check_equal(auto("1000000"), "1000000.0", "AUTO stays regular at exactly the padding threshold")
    s.check_equal(auto("10000000"), "1.0e+7", "AUTO switches to scientific just past the padding threshold")
    s.check_equal(auto("0.000001"), "0.000001", "AUTO stays regular at exactly the leading-zero threshold")
    s.check_equal(auto("0.00000001"), "1.0e-8", "AUTO switches to scientific just past the leading-zero threshold")
    s.check_equal(axpf_to_str(ctx, str_to_axpf(ctx, "10000000")), auto("10000000"), "plain axp_ftoa matches axp_ftoa_ex(..., AUTO)")

    s.fuzz("random_itoa_roundtrip", 20_000, lambda: [gen_randomi(200)], run_itoa_roundtrip)
    s.fuzz("random_ftoa_roundtrip", 20_000, lambda: [gen_randomf(50, 30)], run_ftoa_roundtrip)
    s.fuzz(
      "random_ftoa_ex_roundtrip", 20_000,
      lambda: (gen_randomf(50, 30), random.choice([AXP_FTOA_REGULAR, AXP_FTOA_SCIENTIFIC, AXP_FTOA_AUTO])),
      run_ftoa_ex_value_roundtrip,
    )
