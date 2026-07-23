import json
import time
from dataclasses import dataclass, field
from pathlib import Path

from tqdm import tqdm

GREEN = "\033[92m"
RED = "\033[91m"
YELLOW = "\033[93m"
BLUE = "\033[94m"
BOLD = "\033[1m"
DIM = "\033[2m"
RESET = "\033[0m"

PREV_FAILED_PATH = Path(__file__).parent / "prev_failed.jsonl"

_FUZZ_REGISTRY = {}

@dataclass
class Failure:
  suite: str
  name: str
  detail: str

@dataclass
class Report:
  passed: int = 0
  failed: int = 0
  skipped: int = 0
  failures: list = field(default_factory=list)
  _start: float = field(default_factory=time.time)

  @property
  def total(self):
    return self.passed + self.failed + self.skipped

  def elapsed(self):
    return time.time() - self._start

  def print_summary(self):
    print()
    print(f"{BOLD}{'=' * 60}{RESET}")
    if self.failures:
      print(f"{BOLD}Failures:{RESET}")
      for f in self.failures:
        print(f"  {RED}[{f.suite}] {f.name}{RESET}")
        for line in f.detail.splitlines():
          print(f"      {DIM}{line}{RESET}")
      print()
    color = RED if self.failed else GREEN
    status = "FAILED" if self.failed else "PASSED"
    print(
      f"{color}{BOLD}{status}{RESET}  "
      f"{self.passed} passed, {self.failed} failed, {self.skipped} skipped "
      f"({self.total} total) in {self.elapsed():.1f}s"
    )
    print(f"{BOLD}{'=' * 60}{RESET}")


class Suite:
  def __init__(self, name, report: Report):
    self.name = name
    self.report = report
    self._local_pass = 0
    self._local_fail = 0

  def __enter__(self):
    print(f"\n{BOLD}{BLUE}== {self.name} =={RESET}")
    return self

  def __exit__(self, exc_type, exc, tb):
    color = RED if self._local_fail else GREEN
    print(f"  {color}{self._local_pass}/{self._local_pass + self._local_fail} checks passed{RESET}")
    return False

  def check(self, ok, label, detail=""):
    if ok:
      self.report.passed += 1
      self._local_pass += 1
    else:
      self.report.failed += 1
      self._local_fail += 1
      self.report.failures.append(Failure(self.name, label, detail))
      print(f"  {RED}FAIL{RESET} {label}")
      if detail:
        print(f"       {DIM}{detail}{RESET}")

  def check_equal(self, actual, expected, label):
    self.check(actual == expected, label, f"expected {expected!r}, got {actual!r}")

  def check_raises(self, ok, ctx, expected_err, label, err_name_fn, err_str_fn):
    if ok:
      self.check(False, label, "expected the call to fail, but it succeeded")
      return
    detail = f"expected err={expected_err}, got err={ctx.err} ({err_name_fn(ctx)}: {err_str_fn(ctx)})"
    self.check(ctx.err == expected_err, label, detail)

  # randomized checks
  def fuzz(self, name, iterations, gen_inputs, run_op, max_examples=5):
    full_name = f"{self.name}.{name}"
    _FUZZ_REGISTRY[full_name] = run_op

    fails = 0
    examples = []
    with tqdm(total=iterations, desc=f"  {name}", leave=False, unit="case") as bar:
      for _ in range(iterations):
        inputs = gen_inputs()
        try:
          got, expected, expr = run_op(*inputs)
          ok = got == expected
        except Exception as e:
          got, expected, expr = f"<exception: {e}>", "<no exception>", repr(inputs)
          ok = False
        if not ok:
          fails += 1
          _record_prev_failed(full_name, inputs)
          if len(examples) < max_examples:
            examples.append(f"{expr} -> got {got!r}, expected {expected!r}")
        bar.update(1)

    self.report.passed += iterations - fails
    self._local_pass += iterations - fails
    if fails:
      self.report.failed += fails
      self._local_fail += fails
      detail = "\n".join(examples)
      if fails > len(examples):
        detail += f"\n... and {fails - len(examples)} more"
      self.report.failures.append(Failure(self.name, name, detail))
      print(f"  {RED}FAIL{RESET} {name}: {fails}/{iterations} failed")
      for line in examples:
        print(f"       {DIM}{line}{RESET}")
    else:
      print(f"  {GREEN}PASS{RESET} {name}: {iterations}/{iterations}")

def _load_prev_failed():
  if not PREV_FAILED_PATH.exists():
    return []
  cases = []
  with open(PREV_FAILED_PATH) as f:
    for line in f:
      line = line.strip()
      if line:
        cases.append(json.loads(line))
  return cases


def _record_prev_failed(full_name, inputs):
  entry = json.dumps({"name": full_name, "inputs": list(inputs)})
  existing = set()
  if PREV_FAILED_PATH.exists():
    with open(PREV_FAILED_PATH) as f:
      existing = {l.strip() for l in f if l.strip()}
  if entry in existing:
    return
  with open(PREV_FAILED_PATH, "a") as f:
    f.write(entry + "\n")


def replay_prev_failed(report: Report):
  cases = _load_prev_failed()
  name = "Previously failed cases"
  if not cases:
    print(f"\n{BOLD}{BLUE}== {name} =={RESET}")
    print(f"  {YELLOW}skipped (no recorded cases){RESET}")
    return

  print(f"\n{BOLD}{BLUE}== {name} =={RESET}")
  fails = 0
  stale = 0
  with tqdm(total=len(cases), desc="  replaying", leave=False, unit="case") as bar:
    for case in cases:
      run_op = _FUZZ_REGISTRY.get(case["name"])
      if run_op is None:
        stale += 1
        bar.update(1)
        continue
      try:
        got, expected, expr = run_op(*case["inputs"])
        ok = got == expected
      except Exception as e:
        got, expected, expr = f"<exception: {e}>", "<no exception>", repr(case["inputs"])
        ok = False
      if ok:
        report.passed += 1
      else:
        fails += 1
        report.failed += 1
        report.failures.append(Failure(case["name"], "replay", f"{expr} -> got {got!r}, expected {expected!r}"))
      bar.update(1)

  msg = f"  {len(cases) - fails - stale}/{len(cases) - stale} still passing"
  if stale:
    msg += f" ({stale} stale, test no longer exists)"
  color = RED if fails else GREEN
  print(f"{color}{msg}{RESET}")
