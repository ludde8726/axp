#!/usr/bin/env python3
import sys
import time
from pathlib import Path

sys.set_int_max_str_digits(0)

sys.path.insert(0, str(Path(__file__).parent))

import framework

import test_alloc_copy
import test_int_arith
import test_float_arith
import test_transcendental
import test_conversions
import test_errors

SUITES = [
  test_alloc_copy,
  test_int_arith,
  test_float_arith,
  test_transcendental,
  test_conversions,
  test_errors,
]

def main():
  report = framework.Report()
  print(f"{framework.BOLD}axp test suite{framework.RESET}  ({time.strftime('%Y-%m-%d %H:%M:%S')})")
  for module in SUITES: module.run(report)
  framework.replay_prev_failed(report)
  report.print_summary()
  return 1 if report.failed else 0

if __name__ == "__main__":
  sys.exit(main())
