import ctypes
from pathlib import Path
from typing import List, Any
import subprocess
import random
from tqdm import tqdm
from dataclasses import dataclass, field
import sys

from ctypes import (
  c_uint8, c_uint32, c_int64, c_int, c_size_t, c_char_p, c_char,
  c_bool, c_int8, c_void_p, POINTER, Structure, Union, CFUNCTYPE,
  byref, create_string_buffer
)

sys.set_int_max_str_digits(0)

ROOT_DIR = Path(__file__).parent
LOG_PATH = ROOT_DIR / "test_log.txt"
LIB_DIR = ROOT_DIR / "build" / "lib"
LIB_NAME = "libaxp_.so" 
LIB_PATH = LIB_DIR / LIB_NAME
SOURCE_PATH = ROOT_DIR / "axp.c"

CFLAGS = "-Wall -Wextra -Wconversion -Wsign-conversion -Wsign-compare -Wfloat-equal -Wshadow -Wfloat-conversion -std=c11 -pedantic -g -DAXP_DEBUG_ASSERTS".split(" ")

cmd = [
  "gcc",
  *CFLAGS,
  "-shared",
  "-o" ,
  f"{LIB_PATH}",
  "-fPIC",
  f"{SOURCE_PATH}",
]

print(f"Compiling {SOURCE_PATH}...")
result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
if result.returncode != 0:
  print(f"Compilation failed for {SOURCE_PATH}")
  print("Compiler output:")
  print(result.stderr)

lib = ctypes.CDLL(str(LIB_PATH))

axp_digit_t  = c_uint8
axp_size_t   = c_uint32
axp_exp_t    = c_int64
 
AXP_ErrorCode = c_int

class AXP_Ctx(Structure):
  _fields_ = [
    ("precision", axp_size_t),
    ("err",       AXP_ErrorCode),
    ("err_str",   c_char * 1024),
  ]
 
 
class AXP_Int(Structure):
  _fields_ = [
    ("size",     axp_size_t),
    ("capacity", axp_size_t),
    ("digits",   POINTER(axp_digit_t)),
    ("sign",     c_uint8),
  ]

  def __repr__(self):
    return axp_itoa_alloc(byref(ctx), byref(self)).decode()
 
 
class AXP_Float(Structure):
  _fields_ = [
    ("size",     axp_size_t),
    ("capacity", axp_size_t),
    ("digits",   POINTER(axp_digit_t)),
    ("sign",     c_uint8),
    ("exponent", axp_exp_t),
  ]

  def __repr__(self):
    return axp_ftoa_alloc(byref(ctx), byref(self)).decode()

class AxpFxn:
  def __init__(self, func_name, rettype, *argtypes) -> None:
    self.name = func_name
    try: self.func = getattr(lib, self.name)
    except AttributeError:
       print(f"Could not access funciton '{self.name}' from '{LIB_NAME}")
       return
    self.func.restype  = rettype
    self.func.argtypes = list(argtypes)

  def __call__(self, *args):
    return self.func(*args)

# TODO: Don't even know why i made these classes, just make funcions instead, then i can even add a signature for some type checking
axp_freei = AxpFxn("axp_freei", None, POINTER(AXP_Int))
axp_feeef = AxpFxn("axp_freef", None, POINTER(AXP_Float))

axp_addi = AxpFxn("axp_addi", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Int), POINTER(AXP_Int), POINTER(AXP_Int))
axp_subi = AxpFxn("axp_subi", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Int), POINTER(AXP_Int), POINTER(AXP_Int))
axp_muli = AxpFxn("axp_muli", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Int), POINTER(AXP_Int), POINTER(AXP_Int))
axp_divi = AxpFxn("axp_divi", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Int), POINTER(AXP_Int), POINTER(AXP_Int), POINTER(AXP_Int))
axp_powi = AxpFxn("axp_powi", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Int),  axp_size_t, POINTER(AXP_Int))

axp_addf = AxpFxn("axp_addf", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), POINTER(AXP_Float), POINTER(AXP_Float))
axp_subf = AxpFxn("axp_subf", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), POINTER(AXP_Float), POINTER(AXP_Float))
axp_mulf = AxpFxn("axp_mulf", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), POINTER(AXP_Float), POINTER(AXP_Float))
axp_divf = AxpFxn("axp_divf", c_bool, POINTER(AXP_Ctx), POINTER(AXP_Float), POINTER(AXP_Float), POINTER(AXP_Float))

axp_itoa_alloc = AxpFxn("axp_itoa_alloc", c_char_p, POINTER(AXP_Ctx),  POINTER(AXP_Int))

axp_itoa = AxpFxn("axp_itoa", c_size_t, POINTER(AXP_Int), c_void_p, c_size_t)
axp_atoi = AxpFxn("axp_atoi",       c_bool,   POINTER(AXP_Ctx),  c_char_p, POINTER(AXP_Int))

axp_ftoa_alloc = AxpFxn("axp_ftoa_alloc", c_char_p, POINTER(AXP_Ctx),  POINTER(AXP_Float))
axp_atof = AxpFxn("axp_atof",       c_bool,   POINTER(AXP_Ctx),  c_char_p, POINTER(AXP_Float))

axp_strerror = AxpFxn("axp_strerror",    c_char_p, POINTER(AXP_Ctx))

ctx = AXP_Ctx()
ctx.precision = 50

def gen_randomi(max_sz, only_pos=False):
  sz = random.randint(1, max_sz)
  val = random.randint(10**(sz - 1), 10**sz - 1)
  if random.randint(0, 1) and not only_pos: val = -val
  return val

def int_to_axpi(x):
  axp_x = AXP_Int()
  if not axp_atoi(byref(ctx), str(x).encode(), byref(axp_x)): raise Exception(axp_strerror(byref(ctx)))
  return axp_x

def axpi_to_int(x):
  needed = axp_itoa(byref(x), None, 0)
  buf = create_string_buffer(needed)
  axp_itoa(byref(x), buf, needed)
  return int(buf.value.decode())


GREEN = "\033[92m"
RED   = "\033[91m"
RESET = "\033[0m"

@dataclass
class TestResults:
  total: int = 0
  failed: int = 0
  skipped: int = 0
  failed_cases: list[str] = field(default_factory=list)

  @property
  def passed(self):
    return self.total - self.failed

def _run_add(x, y):
  axp_x, axp_y, axp_res = int_to_axpi(x), int_to_axpi(y), AXP_Int()
  if not axp_addi(byref(ctx), byref(axp_x), byref(axp_y), byref(axp_res)):
    raise Exception(axp_strerror(byref(ctx)))
  result = axpi_to_int(axp_res)
  axp_freei(byref(axp_x)); axp_freei(byref(axp_y)); axp_freei(byref(axp_res))
  return result, x + y, f"{x} + {y}"

def _run_sub(x, y):
  axp_x, axp_y, axp_res = int_to_axpi(x), int_to_axpi(y), AXP_Int()
  if not axp_subi(byref(ctx), byref(axp_x), byref(axp_y), byref(axp_res)):
    raise Exception(axp_strerror(byref(ctx)))
  result = axpi_to_int(axp_res)
  axp_freei(byref(axp_x)); axp_freei(byref(axp_y)); axp_freei(byref(axp_res))
  return result, x - y, f"{x} - {y}"

def _run_mul(x, y):
  axp_x, axp_y, axp_res = int_to_axpi(x), int_to_axpi(y), AXP_Int()
  if not axp_muli(byref(ctx), byref(axp_x), byref(axp_y), byref(axp_res)):
    raise Exception(axp_strerror(byref(ctx)))
  result = axpi_to_int(axp_res)
  axp_freei(byref(axp_x)); axp_freei(byref(axp_y)); axp_freei(byref(axp_res))
  return result, x * y, f"{x} * {y}"

def _run_div(x, y):
  axp_x, axp_y = int_to_axpi(x), int_to_axpi(y)
  axp_res, axp_rem = AXP_Int(), AXP_Int()
  if not axp_divi(byref(ctx), byref(axp_x), byref(axp_y), byref(axp_res), byref(axp_rem)):
    raise Exception(axp_strerror(byref(ctx)))
  res, rem = axpi_to_int(axp_res), axpi_to_int(axp_rem)
  axp_freei(byref(axp_x)); axp_freei(byref(axp_y))
  axp_freei(byref(axp_res)); axp_freei(byref(axp_rem))
  actual = abs(x) // abs(y)
  if (x < 0) != (y < 0): actual = -actual
  return (res, rem), (actual, x - actual * y), f"{x} / {y}"

def _run_pow(x, y):
  axp_x, axp_res = int_to_axpi(x), AXP_Int()
  if not axp_powi(byref(ctx), byref(axp_x), y, byref(axp_res)):
    raise Exception(axp_strerror(byref(ctx)))
  result = axpi_to_int(axp_res)
  axp_freei(byref(axp_x)); axp_freei(byref(axp_res))
  return result, x ** y, f"{x} ^ {y}"


# ─── Input generators ─────────────────────────────────────────────────────────

def _gen_binary(bits):
  return lambda: (gen_randomi(bits), gen_randomi(bits))

def _gen_div(bits):
  def gen():
    x = gen_randomi(bits)
    y = 0
    while y == 0: y = gen_randomi(bits)
    return x, y
  return gen

def _gen_pow(base_bits, exp_max):
  def gen():
    x, y = 0, 0
    while x == 0 and y == 0:
        x = gen_randomi(base_bits)
        y = random.randint(0, exp_max)
    return x, y
  return gen

def _gen_big_pow(base_bits, exp_bits):
  def gen():
    x, y = 0, 0
    while x == 0 and y == 0:
      x = gen_randomi(base_bits)
      y = gen_randomi(exp_bits, only_pos=True)
    return x, y
  return gen

def _format_failure(name, expr, got, expected):
  if isinstance(got, tuple):
    res, rem = got
    actual_res, actual_rem = expected
    return (
      f"Test case '{name}':\n  {expr} "
      f"{'!=' if res != actual_res else '='} {res}\n"
      f"  Actual result: {actual_res}\n  Diff: {actual_res - res}\n"
      f"  Remainder: {rem}\n  Actual remainder: {actual_rem}\n"
      f"  Diff: {actual_rem - rem}"
    )
  return (
    f"Test case '{name}':\n  {expr} != {got}\n"
    f"  Actual result: {expected}\n  Diff: {expected - got}"
  )

def _run_test(name, iterations, gen_inputs, run_op, results: TestResults, exact_equals=True):
  i = 0
  with tqdm(total=iterations, leave=False) as t:
    t.set_description(name)
    for i in range(iterations):
      inputs = gen_inputs()
      got, expected, expr = run_op(*inputs)
      if exact_equals and got != expected:
        results.failed_cases.append(_format_failure(name, expr, got, expected))
        results.failed += 1
        results.total += i + 1
        results.skipped += iterations - i - 1
        t.close()
        print(f"{name}: {RED}Failed{RESET}")
        return
      elif not exact_equals:
        pass # TODO: somehow check float equals
      t.update(1)

  results.total += iterations
  print(f"{name}: {GREEN}Success{RESET}")

def run_all_tests() -> TestResults:
  tests = [
    ("Random Integer Add",     100_000, _gen_binary(100),    _run_add),
    ("Random Integer Sub",     100_000, _gen_binary(100),    _run_sub),
    ("Random Integer Mul",     100_000, _gen_binary(100),    _run_mul),
    ("Random Integer Div",     100_000, _gen_div(100),       _run_div),
    ("Random Integer Pow",     100_000, _gen_pow(3, 20),     _run_pow),
    ("Big Random Integer Pow", 250,     _gen_big_pow(14, 3), _run_pow),
  ]

  results = TestResults()
  for name, iterations, gen_inputs, run_op in tests:
    _run_test(name, iterations, gen_inputs, run_op, results)

  color = RED if results.failed > 0 else GREEN
  print(f"\n{color}Ran {results.total} tests with {results.failed} failure(s) and {results.skipped} skipped{RESET}")

  return results

results = run_all_tests()
if results.failed:
  with open(str(LOG_PATH), 'w') as f:
    f.write("\n\n".join(results.failed_cases))