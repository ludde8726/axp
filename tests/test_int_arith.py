import random
from ctypes import byref

import framework
from axp_bindings import AXP_Int, new_ctx, axp_addi, axp_subi, axp_muli, axp_divi, axp_powi, axp_freei, int_to_axpi, axpi_to_int
from helpers import gen_randomi, gen_nonzero_int

ctx = new_ctx(precision=16)

def run_add(x, y):
  ax, ay, ar = int_to_axpi(ctx, x), int_to_axpi(ctx, y), AXP_Int()
  axp_addi(byref(ctx), byref(ax), byref(ay), byref(ar))
  got = axpi_to_int(ar)
  axp_freei(byref(ax)); axp_freei(byref(ay)); axp_freei(byref(ar))
  return got, x + y, f"{x} + {y}"

def run_sub(x, y):
  ax, ay, ar = int_to_axpi(ctx, x), int_to_axpi(ctx, y), AXP_Int()
  axp_subi(byref(ctx), byref(ax), byref(ay), byref(ar))
  got = axpi_to_int(ar)
  axp_freei(byref(ax)); axp_freei(byref(ay)); axp_freei(byref(ar))
  return got, x - y, f"{x} - {y}"

def run_mul(x, y):
  ax, ay, ar = int_to_axpi(ctx, x), int_to_axpi(ctx, y), AXP_Int()
  axp_muli(byref(ctx), byref(ax), byref(ay), byref(ar))
  got = axpi_to_int(ar)
  axp_freei(byref(ax)); axp_freei(byref(ay)); axp_freei(byref(ar))
  return got, x * y, f"{x} * {y}"

def run_div(x, y):
  ax, ay = int_to_axpi(ctx, x), int_to_axpi(ctx, y)
  ar, arem = AXP_Int(), AXP_Int()
  axp_divi(byref(ctx), byref(ax), byref(ay), byref(ar), byref(arem))
  got = (axpi_to_int(ar), axpi_to_int(arem))
  axp_freei(byref(ax)); axp_freei(byref(ay)); axp_freei(byref(ar)); axp_freei(byref(arem))
  expected_q = abs(x) // abs(y)
  if (x < 0) != (y < 0):
    expected_q = -expected_q
  expected_r = x - expected_q * y
  return got, (expected_q, expected_r), f"{x} / {y}"

def run_pow(x, y):
  ax, ar = int_to_axpi(ctx, x), AXP_Int()
  axp_powi(byref(ctx), byref(ax), y, byref(ar))
  got = axpi_to_int(ar)
  axp_freei(byref(ax)); axp_freei(byref(ar))
  return got, x ** y, f"{x} ^ {y}"

def run(report):
  with framework.Suite("int_arith", report) as s:
    # edge cases
    s.check_equal(run_add(0, 0)[0], 0, "0 + 0 = 0")
    s.check_equal(run_add(-5, 5)[0], 0, "-5 + 5 = 0")
    s.check_equal(run_add(999999999999999999, 1)[0], 1000000000000000000, "carry propagates through all 9s")
    s.check_equal(run_sub(5, 5)[0], 0, "5 - 5 = 0")
    s.check_equal(run_sub(0, 5)[0], -5, "0 - 5 = -5")
    s.check_equal(run_sub(1000000000000000000, 1)[0], 999999999999999999, "borrow propagates through all 0s")
    s.check_equal(run_mul(0, 12345)[0], 0, "0 * n = 0")
    s.check_equal(run_mul(-3, -3)[0], 9, "negative * negative = positive")
    s.check_equal(run_mul(-3, 3)[0], -9, "negative * positive = negative")
    s.check_equal(run_div(7, 2)[0], (3, 1), "7 / 2 = 3 remainder 1")
    s.check_equal(run_div(-7, 2)[0], (-3, -1), "-7 / 2 = -3 remainder -1 (a = bq + r)")
    s.check_equal(run_div(7, -2)[0], (-3, 1), "7 / -2 = -3 remainder 1")
    s.check_equal(run_div(-7, -2)[0], (3, -1), "-7 / -2 = 3 remainder -1")
    s.check_equal(run_div(6, 2)[0], (3, 0), "exact division has 0 remainder")
    s.check_equal(run_div(0, 5)[0], (0, 0), "0 / n = 0 remainder 0")
    s.check_equal(run_pow(5, 0)[0], 1, "n^0 = 1")
    s.check_equal(run_pow(0, 5)[0], 0, "0^n = 0 for positive n")
    s.check_equal(run_pow(1, 1000)[0], 1, "1^n = 1")
    s.check_equal(run_pow(-2, 3)[0], -8, "negative base, odd exponent -> negative")
    s.check_equal(run_pow(-2, 4)[0], 16, "negative base, even exponent -> positive")
    s.check_equal(run_pow(2, 64)[0], 2 ** 64, "pow beyond native int64 range")

    s.fuzz("random_add", 20_000, lambda: (gen_randomi(80), gen_randomi(80)), run_add)
    s.fuzz("random_sub", 20_000, lambda: (gen_randomi(80), gen_randomi(80)), run_sub)
    s.fuzz("random_mul", 20_000, lambda: (gen_randomi(80), gen_randomi(80)), run_mul)
    s.fuzz("random_div", 20_000, lambda: (gen_randomi(80), gen_nonzero_int(80)), run_div)
    s.fuzz("random_pow_small_exp", 20_000, lambda: (gen_randomi(3), random.randint(0, 20)), run_pow)
    s.fuzz("random_pow_large_base", 250, lambda: (gen_randomi(14), random.randint(0, 999)), run_pow)
