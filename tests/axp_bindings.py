import ctypes
import subprocess
import sys
from pathlib import Path
from ctypes import (
  c_uint8, c_int8, c_uint32, c_int64, c_int, c_size_t, c_char_p, c_char,
  byref, c_bool, c_void_p, POINTER, Structure, create_string_buffer,
)

ROOT_DIR = Path(__file__).parent.parent
BUILD_DIR = Path(__file__).parent / "build"
LIB_PATH = BUILD_DIR / "libaxp_tests.so"
SOURCE_PATH = ROOT_DIR / "axp.c"
HEADER_PATH = ROOT_DIR / "axp.h"

CFLAGS = (
  "-Wall -Wextra -Wconversion -Wsign-conversion -Wsign-compare "
  "-Wfloat-equal -Wshadow -Wfloat-conversion -std=c11 -pedantic -g "
  "-DAXP_DEBUG_ASSERTS"
).split()

def _needs_rebuild():
  if not LIB_PATH.exists(): return True
  lib_mtime = LIB_PATH.stat().st_mtime
  return any(p.stat().st_mtime > lib_mtime for p in (SOURCE_PATH, HEADER_PATH))

def _build():
  BUILD_DIR.mkdir(exist_ok=True)
  if not _needs_rebuild(): return
  cmd = ["gcc", *CFLAGS, "-shared", "-fPIC", "-o", str(LIB_PATH), str(SOURCE_PATH)]
  print(f"Compiling {SOURCE_PATH.name} -> {LIB_PATH} ...")
  result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
  if result.returncode != 0:
    print(f"Compilation failed for {SOURCE_PATH}:")
    print(result.stderr)
    sys.exit(1)
  if result.stderr:
    print(result.stderr)

_build()

lib = ctypes.CDLL(str(LIB_PATH))

axp_digit_t = c_uint8
axp_size_t = c_uint32
axp_exp_t = c_int64
AXP_ErrorCode = c_int

AXP_OK = 0
AXP_ERR_ALLOC = 1
AXP_ERR_DIV_ZERO = 2
AXP_ERR_OVERFLOW = 3
AXP_ERR_ROUNDING = 4
AXP_ERR_PARSE = 5
AXP_ERR_UNINITIALIZED = 6
AXP_ERR_FORMAT = 7
AXP_ERR_WRITE = 8

ERROR_NAMES = {
  AXP_OK: "AXP_OK",
  AXP_ERR_ALLOC: "AXP_ERR_ALLOC",
  AXP_ERR_DIV_ZERO: "AXP_ERR_DIV_ZERO",
  AXP_ERR_OVERFLOW: "AXP_ERR_OVERFLOW",
  AXP_ERR_ROUNDING: "AXP_ERR_ROUNDING",
  AXP_ERR_PARSE: "AXP_ERR_PARSE",
  AXP_ERR_UNINITIALIZED: "AXP_ERR_UNINITIALIZED",
  AXP_ERR_FORMAT: "AXP_ERR_FORMAT",
  AXP_ERR_WRITE: "AXP_ERR_WRITE",
}

AXP_FTOA_REGULAR = 0
AXP_FTOA_SCIENTIFIC = 1
AXP_FTOA_AUTO = 2

class AXP_Ctx(Structure):
  _fields_ = [
    ("precision", axp_size_t),
    ("err", AXP_ErrorCode),
    ("err_str", c_char * 1024),
    ("fast_rounding", c_bool),
    ("ziv_safety_digits", axp_size_t),
    ("ziv_max_retries", axp_size_t),
  ]

class AXP_Int(Structure):
  _fields_ = [
    ("size", axp_size_t),
    ("capacity", axp_size_t),
    ("digits", POINTER(axp_digit_t)),
    ("sign", c_uint8),
  ]

class AXP_Float(Structure):
  _fields_ = [
    ("size", axp_size_t),
    ("capacity", axp_size_t),
    ("digits", POINTER(axp_digit_t)),
    ("sign", c_uint8),
    ("exponent", axp_exp_t),
  ]

def _fn(name, restype, *argtypes):
  f = getattr(lib, name)
  f.restype = restype
  f.argtypes = list(argtypes)
  return f

axp_initi = _fn("axp_initi", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Int), axp_size_t)
axp_initf = _fn("axp_initf", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float))
axp_initf_ex = _fn("axp_initf_ex", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), axp_size_t)
axp_realloci = _fn("axp_realloci", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Int), axp_size_t)
axp_reallocf = _fn("axp_reallocf", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), axp_size_t)
axp_reallocf_round = _fn("axp_reallocf_round", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), axp_size_t)
axp_freei = _fn("axp_freei", None, POINTER(AXP_Int))
axp_freef = _fn("axp_freef", None, POINTER(AXP_Float))

axp_copyi = _fn("axp_copyi", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Int), POINTER(AXP_Int))
axp_copyi_ex = _fn("axp_copyi_ex", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Int), POINTER(AXP_Int), axp_size_t)
axp_copyf = _fn("axp_copyf", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), POINTER(AXP_Float))
axp_copyf_ex = _fn("axp_copyf_ex", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), POINTER(AXP_Float), axp_size_t)
axp_copyf_ex_round = _fn("axp_copyf_ex_round", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), POINTER(AXP_Float), axp_size_t)
axp_copyf_exact = _fn("axp_copyf_exact", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), POINTER(AXP_Float))

axp_abs_cmpi = _fn("axp_abs_cmpi", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Int), POINTER(AXP_Int), POINTER(c_int8))
axp_is_zeroi = _fn("axp_is_zeroi", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Int), POINTER(c_bool))
axp_normalizef = _fn("axp_normalizef", None, POINTER(AXP_Float))
axp_roundf = _fn("axp_roundf", None, POINTER(AXP_Float), axp_size_t)
axp_floorf = _fn("axp_floorf", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), POINTER(AXP_Int), POINTER(AXP_Float))

axp_shli = _fn("axp_shli", None, POINTER(AXP_Ctx), POINTER(AXP_Int), axp_size_t)
axp_shri = _fn("axp_shri", None, POINTER(AXP_Ctx), POINTER(AXP_Int), axp_size_t)

axp_addi = _fn("axp_addi", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Int), POINTER(AXP_Int), POINTER(AXP_Int))
axp_subi = _fn("axp_subi", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Int), POINTER(AXP_Int), POINTER(AXP_Int))
axp_muli = _fn("axp_muli", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Int), POINTER(AXP_Int), POINTER(AXP_Int))
axp_divi = _fn("axp_divi", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Int), POINTER(AXP_Int), POINTER(AXP_Int), POINTER(AXP_Int))
axp_powi = _fn("axp_powi", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Int), axp_size_t, POINTER(AXP_Int))

axp_addf = _fn("axp_addf", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), POINTER(AXP_Float), POINTER(AXP_Float))
axp_addf_ex = _fn("axp_addf_ex", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), POINTER(AXP_Float), POINTER(AXP_Float), axp_size_t)
axp_subf = _fn("axp_subf", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), POINTER(AXP_Float), POINTER(AXP_Float))
axp_subf_ex = _fn("axp_subf_ex", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), POINTER(AXP_Float), POINTER(AXP_Float), axp_size_t)
axp_mulf = _fn("axp_mulf", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), POINTER(AXP_Float), POINTER(AXP_Float))
axp_mulf_ex = _fn("axp_mulf_ex", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), POINTER(AXP_Float), POINTER(AXP_Float), axp_size_t)
axp_divf = _fn("axp_divf", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), POINTER(AXP_Float), POINTER(AXP_Float))
axp_divf_ex = _fn("axp_divf_ex", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), POINTER(AXP_Float), POINTER(AXP_Float), axp_size_t)
axp_powf = _fn("axp_powf", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), axp_exp_t, POINTER(AXP_Float))
axp_powf_ex = _fn("axp_powf_ex", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), axp_exp_t, POINTER(AXP_Float), axp_size_t)

axp_e = _fn("axp_e", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float))
axp_e_ex = _fn("axp_e_ex", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), axp_size_t)
axp_expf = _fn("axp_expf", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), POINTER(AXP_Float))
axp_expf_ex = _fn("axp_expf_ex", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), POINTER(AXP_Float), axp_size_t)
axp_expf_no_splitting = _fn("axp_expf_no_splitting", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), POINTER(AXP_Float), axp_size_t)
axp_lnf = _fn("axp_lnf", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), POINTER(AXP_Float))
axp_lnf_ex = _fn("axp_lnf_ex", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), POINTER(AXP_Float), axp_size_t)
axp_powff = _fn("axp_powff", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), POINTER(AXP_Float), POINTER(AXP_Float))
axp_powff_ex = _fn("axp_powff_ex", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), POINTER(AXP_Float), POINTER(AXP_Float), axp_size_t)

axp_itoa = _fn("axp_itoa", c_size_t, POINTER(AXP_Int), c_void_p, c_size_t)
axp_itoa_alloc = _fn("axp_itoa_alloc", c_char_p, POINTER(AXP_Ctx), POINTER(AXP_Int))
axp_atoi = _fn("axp_atoi", c_bool, POINTER(AXP_Ctx), c_char_p, POINTER(AXP_Int))
axp_ftoa = _fn("axp_ftoa", c_size_t, POINTER(AXP_Float), c_void_p, c_size_t)
axp_ftoa_ex = _fn("axp_ftoa_ex", c_size_t, POINTER(AXP_Float), c_void_p, c_size_t, c_int)
axp_ftoa_alloc = _fn("axp_ftoa_alloc", c_char_p, POINTER(AXP_Ctx), POINTER(AXP_Float))
axp_atof = _fn("axp_atof", c_bool, POINTER(AXP_Ctx), c_char_p, POINTER(AXP_Float))

axp_strerror = _fn("axp_strerror", c_char_p, POINTER(AXP_Ctx))
axp_error_reset = _fn("axp_error_reset", None, POINTER(AXP_Ctx))


def new_ctx(precision=16, fast_rounding=False):
  ctx = AXP_Ctx()
  ctx.precision = precision
  ctx.fast_rounding = fast_rounding
  return ctx

def err_name(ctx):
  return ERROR_NAMES.get(ctx.err, f"<unknown err {ctx.err}>")

def err_str(ctx):
  return axp_strerror(byref(ctx)).decode()

def int_to_axpi(ctx, x):
  axp_x = AXP_Int()
  if not axp_atoi(byref(ctx), str(x).encode(), byref(axp_x)):
    raise RuntimeError(f"axp_atoi({x!r}) failed: {err_str(ctx)}")
  return axp_x

def axpi_to_int(x):
  needed = axp_itoa(byref(x), None, 0) + 1  # +1 for the null terminator
  buf = create_string_buffer(needed)
  axp_itoa(byref(x), buf, needed)
  return int(buf.value.decode())

def str_to_axpf(ctx, s):
  axp_x = AXP_Float()
  if not axp_atof(byref(ctx), s.encode(), byref(axp_x)):
    raise RuntimeError(f"axp_atof({s!r}) failed: {err_str(ctx)}")
  return axp_x

def axpf_to_str(ctx, x):
  return axp_ftoa_alloc(byref(ctx), byref(x)).decode()

def axpf_to_str_ex(x, fmt):
  needed = axp_ftoa_ex(byref(x), None, 0, fmt) + 1  # +1 for the null terminator
  buf = create_string_buffer(needed)
  axp_ftoa_ex(byref(x), buf, needed, fmt)
  return buf.value.decode()
