import os
import sys
import subprocess
import json
import time
from pathlib import Path
from typing import Iterable, Any, Optional

ROOT_DIR = Path(__file__).parent
TESTS_DIR = ROOT_DIR / "tests"
BIN_DIR = ROOT_DIR / "build" / "bin"
LIB_DIR = ROOT_DIR / "build" / "lib"
DATA_FILE = TESTS_DIR / "test_data.json"
LIB_NAME = "axp"


def find_c_files() -> Iterable[Path]: return sorted(TESTS_DIR.glob("*.c"))

def compile_test(source_path: Path) -> Path:
  exe_name = source_path.stem
  exe_path = BIN_DIR / exe_name

  cmd = [
    "gcc",
    str(source_path),
    f"-I{ROOT_DIR}",
    f"-L{LIB_DIR}",
    f"-l{LIB_NAME}",
    "-fsanitize=address,float-divide-by-zero,signed-integer-overflow",
    "-g",
    "-o",
    str(exe_path),
  ]

  print(f"Compiling {source_path.name} ...")
  result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
  if result.returncode != 0:
    print(f"Compilation failed for {source_path.name}")
    print("Compiler output:")
    print(result.stderr)
    sys.exit(1)

  return exe_path

def run_test(exe_path: Path) -> dict[str, str | int]:
  result = subprocess.run(
    [str(exe_path)],
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE
  )
  return {
    "stdout": result.stdout.decode("utf-8", errors="replace"),
    "stderr": result.stderr.decode("utf-8", errors="replace"),
    "returncode": result.returncode,
  }

def load_test_data() -> dict[Any, Any]:
  if not DATA_FILE.exists(): return {}
  with open(DATA_FILE, "r", encoding="utf-8") as f: return json.load(f)

def save_test_data(data: dict[Any, Any]):
  with open(DATA_FILE, "w", encoding="utf-8") as f: json.dump(data, f, indent=2, ensure_ascii=False)

def record_mode(test_name: Optional[str] = None) -> None:
  sources = find_c_files()
  if test_name:
    sources = [t for t in sources if t.stem == test_name]
    if not sources:
      print(f"Test '{test_name}' not found.")
      sys.exit(1)
  data = {}
  for src in sources:
    exe = compile_test(src)
    print(f"Running {exe.name} ...")
    result = run_test(exe)
    data[src.stem] = {
      "timestamp": time.time(),
      "stdout": result["stdout"],
      "stderr": result["stderr"],
      "returncode": result["returncode"],
    }
    print(f"Recorded {src.stem}")
  save_test_data(data)

def compare_mode(test_name: Optional[str] = None) -> None:
  stored = load_test_data()
  if not stored:
    print("No stored test data found. Run with `tests.py record` first.")
    sys.exit(1)
  sources = find_c_files()
  if test_name:
    sources = [t for t in sources if t.stem == test_name]
    if not sources:
      print(f"Test '{test_name}' not found.")
      sys.exit(1)
  passed, failed = 0, 0

  for src in sources:
    exe = compile_test(src)
    print(f"Running {exe.name} ...")
    result = run_test(exe)
    ref = stored.get(src.stem)
    if not ref:
      print(f"No stored data for {src.stem}. Skipping.")
      continue
    if (result["stdout"] == ref["stdout"] and result["stderr"] == ref["stderr"] and result["returncode"] == ref["returncode"]):
      print(f"  PASS")
      passed += 1
    else:
      print(f"  FAIL")
      print("  --- EXPECTED ---")
      print(f"  STDOUT:\n{ref['stdout']}" if ref['stdout'] else "  STDOUT:")
      print(f"  STDERR:\n{ref['stderr']}" if ref['stderr'] else "  STDERR:")
      print(f"  RETURNCODE: {ref['returncode']}")
      print("  --- GOT ---")
      print(f"  STDOUT:\n{result['stdout']}" if result['stdout'] else "  STDOUT:")
      print(f"  STDERR:\n{result['stderr']}" if result['stderr'] else "  STDERR:")
      print(f"  RETURNCODE: {result['returncode']}\n")
      failed += 1

  print("\n==============================")
  print(f"Passed: {passed}")
  print(f"Failed: {failed}")
  print("==============================")

  sys.exit(0 if failed == 0 else 1)

if __name__ == "__main__":
  args = sys.argv[1:]

  if len(args) == 0: compare_mode()
  elif args[0] == "record": record_mode(args[1] if len(args) > 1 else None)
  else: compare_mode(args[0])