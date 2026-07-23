from ctypes import byref
from decimal import Decimal, getcontext, ROUND_HALF_UP, MAX_EMAX, MIN_EMIN, Context

import framework
from axp_bindings import AXP_Float, new_ctx, axp_e_ex, axp_expf, axp_lnf, axp_powff, axp_freef, str_to_axpf, axpf_to_str
from helpers import gen_randomf

ctx = new_ctx(precision=16)
_REF_GUARD_PREC = 50

getcontext().Emax = MAX_EMAX
getcontext().Emin = MIN_EMIN

def _correctly_rounded(compute, target_prec):
  getcontext().prec = target_prec + _REF_GUARD_PREC
  getcontext().rounding = ROUND_HALF_UP
  v = compute()
  getcontext().prec = target_prec
  getcontext().rounding = ROUND_HALF_UP
  return +v

def run_e_check(prec):
  ar = AXP_Float()
  axp_e_ex(byref(ctx), byref(ar), prec)
  got = Decimal(axpf_to_str(ctx, ar))
  axp_freef(byref(ar))
  expected = _correctly_rounded(lambda: Decimal(1).exp(), prec)
  return got, expected, "e"

def run_expf(x_str):
  ax, ar = str_to_axpf(ctx, x_str), AXP_Float()
  axp_expf(byref(ctx), byref(ax), byref(ar))
  got = Decimal(axpf_to_str(ctx, ar))
  axp_freef(byref(ax)); axp_freef(byref(ar))
  expected = _correctly_rounded(lambda: Decimal(x_str).exp(), ctx.precision)
  return got, expected, f"e^{x_str}"

def run_lnf(x_str):
  ax, ar = str_to_axpf(ctx, x_str), AXP_Float()
  axp_lnf(byref(ctx), byref(ax), byref(ar))
  got = Decimal(axpf_to_str(ctx, ar))
  axp_freef(byref(ax)); axp_freef(byref(ar))
  expected = _correctly_rounded(lambda: Decimal(x_str).ln(), ctx.precision)
  return got, expected, f"ln({x_str})"

def run_powff(x_str, y_str):
  ax, ay, ar = str_to_axpf(ctx, x_str), str_to_axpf(ctx, y_str), AXP_Float()
  axp_powff(byref(ctx), byref(ax), byref(ay), byref(ar))
  got = Decimal(axpf_to_str(ctx, ar))
  axp_freef(byref(ax)); axp_freef(byref(ay)); axp_freef(byref(ar))
  big_ctx = Context(prec=ctx.precision + _REF_GUARD_PREC, Emax=MAX_EMAX, Emin=MIN_EMIN)
  expected_full = big_ctx.power(Decimal(x_str), Decimal(y_str))
  big_ctx.prec = ctx.precision
  big_ctx.rounding = ROUND_HALF_UP
  expected = big_ctx.plus(expected_full)
  return got, expected, f"{x_str} ** {y_str}"

def run(report):
  with framework.Suite("transcendental", report) as s:
    # e
    got, expected, _ = run_e_check(50)
    s.check_equal(got, expected, "e to 50 digits matches Decimal(1).exp()")

    # exp edge cases
    s.check_equal(run_expf("0.0")[0], Decimal("1.000000000000000"), "e^0 = 1")
    s.check_equal(run_expf("1.0")[0], _correctly_rounded(lambda: Decimal(1).exp(), 16), "e^1 = e")
    s.check_equal(run_expf("-1.0")[0], _correctly_rounded(lambda: (-Decimal(1)).exp(), 16), "e^-1 = 1/e")
    got, expected, _ = run_expf("2.0")
    s.check_equal(got, expected, "e^2 matches reference")

    # ln edge cases
    s.check_equal(run_lnf("1.0")[0], Decimal("0"), "ln(1) = 0")
    got, expected, _ = run_lnf("10.0")
    s.check_equal(got, expected, "ln(10) matches reference")
    got, expected, _ = run_lnf("0.5")
    s.check_equal(got, expected, "ln(0.5) is negative and matches reference")
    e_val = AXP_Float()
    axp_e_ex(byref(ctx), byref(e_val), 30)
    e_str = axpf_to_str(ctx, e_val)
    axp_freef(byref(e_val))
    got, expected, _ = run_lnf(e_str)
    s.check(abs(got - Decimal(1)) < Decimal("1e-14"), "ln(e) is approximately 1", f"got {got}")

    # powff edge cases
    s.check_equal(run_powff("2.0", "0.0")[0], Decimal("1.000000000000000"), "x^0 = 1")
    s.check_equal(run_powff("1.0", "3.5")[0], Decimal("1.000000000000000"), "1^y = 1")
    got, expected, _ = run_powff("2.0", "0.5")
    s.check_equal(got, expected, "2^0.5 = sqrt(2), matches reference")
    got, expected, _ = run_powff("4.0", "0.5")
    s.check_equal(got, Decimal("2.000000000000000"), "4^0.5 = 2 exactly")
    got, expected, _ = run_powff("8.0", "0.3333333333333333")
    s.check(abs(got - Decimal(2)) < Decimal("1e-10"), "8^(1/3) is approximately 2", f"got {got}")
    got, expected, _ = run_powff("2.0", "10.0")  # integer-valued y -> exact delegation path
    s.check_equal(got, Decimal("1024.000000000000"), "2^10 via the integer-exponent delegation path")
    got, expected, _ = run_powff("2.0", "-3.0")
    s.check_equal(got, Decimal("0.125000000000000"), "2^-3 via the integer-exponent delegation path")
    got, expected, _ = run_powff("-2.0", "3.0")
    s.check_equal(got, Decimal("-8.00000000000000"), "negative base with integer exponent works")

    s.fuzz("random_exp", 5_000, lambda: [gen_randomf(3, 2)], run_expf)
    s.fuzz("random_ln", 20_000, lambda: [gen_randomf(5, 30, only_pos=True)], run_lnf)
    s.fuzz(
      "random_powff", 3_000,
      lambda: (gen_randomf(5, 15, only_pos=True), gen_randomf(4, 3)),
      run_powff,
    )
