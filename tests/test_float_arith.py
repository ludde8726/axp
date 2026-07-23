import random
from ctypes import byref
from decimal import Decimal, getcontext, ROUND_HALF_UP

import framework
from axp_bindings import AXP_Float, new_ctx, axp_addf, axp_subf, axp_mulf, axp_divf, axp_powf, axp_freef, str_to_axpf, axpf_to_str
from helpers import gen_randomf

ctx = new_ctx(precision=16)
_REF_GUARD_PREC = 50

def _correctly_rounded(compute, target_prec):
  getcontext().prec = target_prec + _REF_GUARD_PREC
  getcontext().rounding = ROUND_HALF_UP
  v = compute()
  getcontext().prec = target_prec
  getcontext().rounding = ROUND_HALF_UP
  return +v

def _binop_expected(x_str, y_str, op):
  getcontext().prec = ctx.precision
  getcontext().rounding = ROUND_HALF_UP
  x = +Decimal(x_str)
  y = +Decimal(y_str)
  return +op(x, y)

def run_add(x_str, y_str):
  ax, ay, ar = str_to_axpf(ctx, x_str), str_to_axpf(ctx, y_str), AXP_Float()
  axp_addf(byref(ctx), byref(ax), byref(ay), byref(ar))
  got = Decimal(axpf_to_str(ctx, ar))
  axp_freef(byref(ax)); axp_freef(byref(ay)); axp_freef(byref(ar))
  expected = _binop_expected(x_str, y_str, lambda a, b: a + b)
  return got, expected, f"{x_str} + {y_str}"

def run_sub(x_str, y_str):
  ax, ay, ar = str_to_axpf(ctx, x_str), str_to_axpf(ctx, y_str), AXP_Float()
  axp_subf(byref(ctx), byref(ax), byref(ay), byref(ar))
  got = Decimal(axpf_to_str(ctx, ar))
  axp_freef(byref(ax)); axp_freef(byref(ay)); axp_freef(byref(ar))
  expected = _binop_expected(x_str, y_str, lambda a, b: a - b)
  return got, expected, f"{x_str} - {y_str}"

def run_mul(x_str, y_str):
  ax, ay, ar = str_to_axpf(ctx, x_str), str_to_axpf(ctx, y_str), AXP_Float()
  axp_mulf(byref(ctx), byref(ax), byref(ay), byref(ar))
  got = Decimal(axpf_to_str(ctx, ar))
  axp_freef(byref(ax)); axp_freef(byref(ay)); axp_freef(byref(ar))
  expected = _binop_expected(x_str, y_str, lambda a, b: a * b)
  return got, expected, f"{x_str} * {y_str}"

def run_div(x_str, y_str):
  ax, ay, ar = str_to_axpf(ctx, x_str), str_to_axpf(ctx, y_str), AXP_Float()
  axp_divf(byref(ctx), byref(ax), byref(ay), byref(ar))
  got = Decimal(axpf_to_str(ctx, ar))
  axp_freef(byref(ax)); axp_freef(byref(ay)); axp_freef(byref(ar))
  expected = _binop_expected(x_str, y_str, lambda a, b: a / b)
  return got, expected, f"{x_str} / {y_str}"

def run_pow(x_str, y):
  ax, ar = str_to_axpf(ctx, x_str), AXP_Float()
  axp_powf(byref(ctx), byref(ax), y, byref(ar))
  got = Decimal(axpf_to_str(ctx, ar))
  axp_freef(byref(ax)); axp_freef(byref(ar))
  expected = _correctly_rounded(lambda: Decimal(x_str) ** int(y), ctx.precision)
  return got, expected, f"{x_str} ** {y}"

def run(report):
  with framework.Suite("float_arith", report) as s:
    # edge cases
    s.check_equal(run_add("0.0", "0.0")[0], Decimal("0.0"), "0.0 + 0.0 = 0.0")
    s.check_equal(run_add("1.5", "-1.5")[0], Decimal("0.0"), "x + -x = 0.0")
    s.check_equal(run_sub("1.5", "1.5")[0], Decimal("0.0"), "x - x = 0.0")
    s.check_equal(run_mul("0.0", "123.456")[0], Decimal("0.0"), "0.0 * n = 0.0")
    s.check_equal(run_mul("1.0", "42.5")[0], Decimal("42.5"), "1.0 * n = n")
    s.check_equal(run_div("0.0", "5.0")[0], Decimal("0.0"), "0.0 / n = 0.0")
    s.check_equal(run_div("1.0", "3.0")[0], _correctly_rounded(lambda: Decimal(1) / Decimal(3), 16), "1/3 rounds correctly at precision boundary")
    s.check_equal(run_pow("5.0", 0)[0], Decimal("1.0"), "x^0 = 1.0")
    s.check_equal(run_pow("0.0", 5)[0], Decimal("0.0"), "0.0^n = 0.0 for positive n")
    s.check_equal(run_pow("2.0", -3)[0], Decimal("0.125"), "negative exponent takes the reciprocal")
    s.check_equal(run_pow("-2.0", 3)[0], Decimal("-8.0"), "negative base, odd exponent -> negative")

    # exact half-way rounding tie (round-half-up): "1.0000000000000005" has
    # exactly 17 significant digits with the 17th being a 5, so at
    # ctx.precision=16 it must round up to ...001.
    got, expected, _ = run_add("1.0000000000000005", "0.0")
    s.check_equal(got, Decimal("1.000000000000001"), "half-way tie rounds up at the target precision")

    s.fuzz("random_add", 20_000, lambda: (gen_randomf(50, 30), gen_randomf(50, 30)), run_add)
    s.fuzz("random_sub", 20_000, lambda: (gen_randomf(50, 30), gen_randomf(50, 30)), run_sub)
    s.fuzz("random_mul", 20_000, lambda: (gen_randomf(50, 30), gen_randomf(50, 30)), run_mul)
    s.fuzz("random_div", 20_000, lambda: (gen_randomf(50, 30), gen_randomf(50, 30, only_pos=True)), run_div)
    s.fuzz("random_pow", 20_000, lambda: (gen_randomf(5, 30), random.randint(-1000, 1000)), run_pow)
