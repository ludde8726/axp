import random

def gen_randomi(max_digits, only_pos=False):
  sz = random.randint(1, max_digits)
  val = random.randint(10 ** (sz - 1), 10 ** sz - 1)
  if random.randint(0, 1) and not only_pos: val = -val
  return val

def gen_randomf(max_digits, max_exp, only_pos=False):
  mantissa = gen_randomi(max_digits, only_pos)
  exp = random.randint(-max_exp, max_exp)
  digits = str(abs(mantissa))
  size = len(digits)
  res = "-" if mantissa < 0 else ""
  if exp >= 0: res += digits + "0" * exp + ".0"
  elif size + exp <= 0: res += "0." + "0" * abs(size + exp) + digits
  else: res += digits[:-(-exp)] + "." + digits[-(-exp):]
  return res

def gen_nonzero_int(max_digits):
  v = 0
  while v == 0: v = gen_randomi(max_digits)
  return v
