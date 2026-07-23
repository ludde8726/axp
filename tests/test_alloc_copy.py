from ctypes import byref
from decimal import Decimal, getcontext, ROUND_HALF_UP

import framework
from axp_bindings import (
  AXP_Int, AXP_Float, new_ctx,
  axp_initi, axp_initf, axp_initf_ex, axp_realloci, axp_reallocf, axp_reallocf_round,
  axp_freei, axp_freef, axp_copyi, axp_copyi_ex, axp_copyf, axp_copyf_ex,
  axp_copyf_ex_round, axp_copyf_exact, axp_normalizef, axp_roundf, axp_floorf,
  axp_shli, axp_shri, int_to_axpi, axpi_to_int, str_to_axpf, axpf_to_str,
)

def run(report):
  ctx = new_ctx(precision=16)

  with framework.Suite("alloc_copy", report) as s:
    # init
    x = AXP_Int()
    s.check(axp_initi(byref(ctx), byref(x), 8), "axp_initi succeeds")
    s.check_equal(axpi_to_int(x), 0, "fresh AXP_Int is 0")
    axp_freei(byref(x))

    f = AXP_Float()
    s.check(axp_initf(byref(ctx), byref(f)), "axp_initf succeeds")
    s.check_equal(axpf_to_str(ctx, f), "0.0", "fresh AXP_Float is 0.0")
    s.check_equal(f.capacity, ctx.precision, "axp_initf uses ctx->precision as capacity")
    axp_freef(byref(f))

    f2 = AXP_Float()
    axp_initf_ex(byref(ctx), byref(f2), 40)
    s.check_equal(f2.capacity, 40, "axp_initf_ex uses the given capacity")
    axp_freef(byref(f2))

    # realloc
    x = int_to_axpi(ctx, 123456)
    s.check(axp_realloci(byref(ctx), byref(x), 20), "axp_realloci grows")
    s.check_equal(axpi_to_int(x), 123456, "axp_realloci preserves value when growing")
    axp_freei(byref(x))

    f = str_to_axpf(ctx, "123.456")
    axp_reallocf(byref(ctx), byref(f), 4)  # truncates (not rounds) to 4 digits
    s.check_equal(axpf_to_str(ctx, f), "123.4", "axp_reallocf truncates without rounding")
    axp_freef(byref(f))

    f = str_to_axpf(ctx, "123.456")
    axp_reallocf_round(byref(ctx), byref(f), 4)  # rounds to 4 digits
    s.check_equal(axpf_to_str(ctx, f), "123.5", "axp_reallocf_round rounds half up")
    axp_freef(byref(f))

    f = str_to_axpf(ctx, "9.999")
    axp_reallocf_round(byref(ctx), byref(f), 1)
    s.check_equal(axpf_to_str(ctx, f), "10.0", "axp_reallocf_round carries through 9.99...->10")
    axp_freef(byref(f))

    # copy (int)
    src = int_to_axpi(ctx, -777)
    dst = AXP_Int()
    axp_copyi(byref(ctx), byref(dst), byref(src))
    s.check_equal(axpi_to_int(dst), -777, "axp_copyi preserves value and sign")
    axp_freei(byref(src)); axp_freei(byref(dst))

    src = int_to_axpi(ctx, 42)
    dst = AXP_Int()
    axp_copyi_ex(byref(ctx), byref(dst), byref(src), 100)
    s.check_equal(dst.capacity, 100, "axp_copyi_ex uses the requested capacity")
    s.check_equal(axpi_to_int(dst), 42, "axp_copyi_ex preserves value")
    axp_freei(byref(src)); axp_freei(byref(dst))

    # copy (float)
    src = str_to_axpf(ctx, "3.14159265358979")
    dst = AXP_Float()
    axp_copyf_ex(byref(ctx), byref(dst), byref(src), 4)
    s.check_equal(axpf_to_str(ctx, dst), "3.141", "axp_copyf_ex truncates (no rounding) past precision")
    axp_freef(byref(src)); axp_freef(byref(dst))

    src = str_to_axpf(ctx, "3.14159265358979")
    dst = AXP_Float()
    axp_copyf_ex_round(byref(ctx), byref(dst), byref(src), 4)
    s.check_equal(axpf_to_str(ctx, dst), "3.142", "axp_copyf_ex_round rounds half up past precision")
    axp_freef(byref(src)); axp_freef(byref(dst))

    src = str_to_axpf(ctx, "3.14159265358979")
    dst = AXP_Float()
    axp_copyf_exact(byref(ctx), byref(dst), byref(src))
    s.check_equal(axpf_to_str(ctx, dst), axpf_to_str(ctx, src), "axp_copyf_exact never drops digits")
    axp_freef(byref(src)); axp_freef(byref(dst))

    # normalize
    f = str_to_axpf(ctx, "100.00")
    axp_normalizef(byref(f))
    s.check_equal(axpf_to_str(ctx, f), "100.0", "axp_normalizef strips redundant zeros")
    axp_freef(byref(f))

    # round (in-place, no realloc)
    f = str_to_axpf(ctx, "1.2345")
    axp_roundf(byref(f), 3)
    s.check_equal(axpf_to_str(ctx, f), "1.23", "axp_roundf rounds down below the halfway digit")
    axp_freef(byref(f))

    f = str_to_axpf(ctx, "1.2355")
    axp_roundf(byref(f), 3)
    s.check_equal(axpf_to_str(ctx, f), "1.24", "axp_roundf rounds up at/above the halfway digit")
    axp_freef(byref(f))

    # floor (int/frac split)
    for s_str in ["7.25", "-7.25", "0.5", "100.0", "-3.0"]:
      f = str_to_axpf(ctx, s_str)
      n = AXP_Int()
      frac = AXP_Float()
      axp_floorf(byref(ctx), byref(f), byref(n), byref(frac))
      d = Decimal(s_str)
      # axp_floorf truncates towards zero (same-sign int/frac split), matching Decimal's int()/remainder.
      expected_n = int(d)
      expected_frac_str = axpf_to_str(ctx, frac)
      getcontext().prec = 30
      getcontext().rounding = ROUND_HALF_UP
      expected_frac = d - expected_n
      s.check_equal(axpi_to_int(n), expected_n, f"axp_floorf({s_str}) integer part")
      s.check(Decimal(expected_frac_str) == expected_frac, f"axp_floorf({s_str}) fractional part",
              f"got {expected_frac_str}, expected {expected_frac}")
      axp_freef(byref(f)); axp_freei(byref(n)); axp_freef(byref(frac))

    # shift
    x = int_to_axpi(ctx, 123)
    axp_realloci(byref(ctx), byref(x), 10)  # give it headroom to shift into
    axp_shli(byref(ctx), byref(x), 2)
    s.check_equal(axpi_to_int(x), 12300, "axp_shli multiplies by 10^shift")
    axp_freei(byref(x))

    x = int_to_axpi(ctx, 12300)
    axp_shri(byref(ctx), byref(x), 2)
    s.check_equal(axpi_to_int(x), 123, "axp_shri divides by 10^shift")
    axp_freei(byref(x))

    x = int_to_axpi(ctx, 999)
    axp_shri(byref(ctx), byref(x), 5)  # shift more than size -> truncates to 0
    s.check_equal(axpi_to_int(x), 0, "axp_shri past the digit count truncates to 0")
    axp_freei(byref(x))
