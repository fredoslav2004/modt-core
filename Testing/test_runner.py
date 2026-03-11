import json
import subprocess
import os
import re
import sys
from pathlib import Path

class TestResult:
    def __init__(self, name, success, message="", details=""):
        self.name = name
        self.success = success
        self.message = message
        self.details = details

    def __str__(self):
        status = "PASS" if self.success else "FAIL"
        return f"[{status}] {self.name}: {self.message}\n{self.details}"

class ModtTester:
    def __init__(self, modt_executable="./modt"):
        self.modt_executable = str(Path(modt_executable).absolute())

    def run_test(self, test_config_path):
        with open(test_config_path, 'r') as f:
            config = json.load(f)

        test_name = config.get("name", test_config_path)
        modt_content = config.get("modt_content", "")
        expected_outputs = config.get("expected_outputs", [])
        args = config.get("args", ["-genDocs"])
        
        # Create temp directory for test
        test_dir = Path("Testing/temp") / Path(test_config_path).stem
        test_dir.mkdir(parents=True, exist_ok=True)
        
        modt_file = test_dir / "test.modt"
        modt_file.write_text(modt_content)

        # Run modt
        cmd = [self.modt_executable, "--input", str(modt_file), "--out-path", str(test_dir)] + args
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
            if result.returncode != 0:
                return TestResult(test_name, False, "MODT execution failed", result.stderr)
        except Exception as e:
            return TestResult(test_name, False, f"Error running MODT: {str(e)}")

        # Verify outputs
        failures = []
        for expected in expected_outputs:
            file_path = test_dir / expected["file"]
            if not file_path.exists():
                failures.append(f"Expected file {expected['file']} not found")
                continue

            content = file_path.read_text()
            for pattern in expected.get("contains", []):
                if not re.search(pattern, content, re.MULTILINE):
                    failures.append(f"Pattern '{pattern}' not found in {expected['file']}")
            
            for pattern in expected.get("not_contains", []):
                if re.search(pattern, content, re.MULTILINE):
                    failures.append(f"Pattern '{pattern}' FOUND (but should not be) in {expected['file']}")

        if failures:
            return TestResult(test_name, False, "Verification failed", "\n".join(failures))
        
        return TestResult(test_name, True, "All checks passed")

def discover_tests(test_dir="Testing/tests"):
    return list(Path(test_dir).glob("*.json"))

if __name__ == "__main__":
    tester = ModtTester()
    test_files = discover_tests()
    
    if not test_files:
        print("No test files found in Testing/tests/")
        sys.exit(0)

    results = []
    print(f"Running {len(test_files)} tests...\n")
    for test_file in test_files:
        result = tester.run_test(test_file)
        results.append(result)
        print(result)

    passed = sum(1 for r in results if r.success)
    print(f"\nSummary: {passed}/{len(results)} passed")
    
    if passed < len(results):
        sys.exit(1)
