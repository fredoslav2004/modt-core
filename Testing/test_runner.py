import difflib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

try:
    import tomllib
except ModuleNotFoundError:
    tomllib = None

class TestResult:
    def __init__(self, name, success, message="", details=""):
        self.name = name
        self.success = success
        self.message = message
        self.details = details

    def __str__(self):
        status = "PASS" if self.success else "FAIL"
        return f"[{status}] {self.name}: {self.message}\n{self.details}"


class TestCase:
    def __init__(self, config_path, name, args, input_path=None, modt_content=None,
                 expected_outputs=None, timeout_seconds=10, metadata=None,
                 commands=None, render_all_puml=False, run_args=None, run_cwd=None):
        self.config_path = Path(config_path)
        self.name = name
        self.args = ["-genDocs"] if args is None else args
        self.input_path = Path(input_path) if input_path else None
        self.modt_content = modt_content
        self.expected_outputs = expected_outputs or []
        self.timeout_seconds = timeout_seconds
        self.metadata = metadata or {}
        self.commands = commands or []
        self.render_all_puml = render_all_puml
        self.run_args = run_args
        self.run_cwd = Path(run_cwd) if run_cwd else None

    @property
    def case_id(self):
        if self.config_path.name == "case.toml":
            return self.config_path.parent.name
        return self.config_path.stem

    @property
    def base_dir(self):
        return self.config_path.parent

class ModtTester:
    def __init__(self, modt_executable="./modt"):
        self.modt_executable = str(Path(modt_executable).absolute())
        self.run_root = Path("Testing/.runs")

    def run_test(self, test_config_path):
        test_case = self.load_test_case(test_config_path)

        test_dir = self.run_root / test_case.case_id
        if test_dir.exists():
            shutil.rmtree(test_dir)
        test_dir.mkdir(parents=True, exist_ok=True)

        modt_file = self.prepare_input_file(test_case, test_dir)

        run_cwd = test_dir / test_case.run_cwd if test_case.run_cwd else test_dir

        if test_case.run_args is not None:
            cmd = [self.modt_executable] + self.resolve_run_args(test_case.run_args, test_dir, modt_file)
        else:
            cmd = [
                self.modt_executable,
                "--input",
                str(modt_file.resolve()),
                "--out-path",
                str(test_dir.resolve()),
            ] + test_case.args

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=test_case.timeout_seconds,
                cwd=run_cwd,
            )
            if result.returncode != 0:
                return TestResult(test_case.name, False, "MODT execution failed", result.stderr)
        except Exception as e:
            return TestResult(test_case.name, False, f"Error running MODT: {str(e)}")

        # Verify outputs
        failures = []
        for expected in test_case.expected_outputs:
            file_path = test_dir / expected["file"]
            if not file_path.exists():
                failures.append(f"Expected file {expected['file']} not found")
                continue

            content = file_path.read_text()
            snapshot_path = self.resolve_snapshot_path(test_case, expected)
            if snapshot_path:
                snapshot_content = snapshot_path.read_text()
                if content != snapshot_content:
                    diff = "\n".join(
                        difflib.unified_diff(
                            snapshot_content.splitlines(),
                            content.splitlines(),
                            fromfile=str(snapshot_path),
                            tofile=str(file_path),
                            lineterm="",
                        )
                    )
                    failures.append(f"Snapshot mismatch for {expected['file']}\n{diff}")

            for text in expected.get("contains", []):
                if text not in content:
                    failures.append(f"Text '{text}' not found in {expected['file']}")

            for text in expected.get("not_contains", []):
                if text in content:
                    failures.append(f"Text '{text}' was found in {expected['file']}")

            for pattern in expected.get("matches", []):
                if not re.search(pattern, content, re.MULTILINE):
                    failures.append(f"Pattern '{pattern}' not found in {expected['file']}")
            
            for pattern in expected.get("not_matches", []):
                if re.search(pattern, content, re.MULTILINE):
                    failures.append(f"Pattern '{pattern}' FOUND (but should not be) in {expected['file']}")

        failures.extend(self.run_commands(test_case, test_dir))

        if test_case.render_all_puml:
            failures.extend(self.render_all_puml_outputs(test_dir))

        if failures:
            return TestResult(test_case.name, False, "Verification failed", "\n".join(failures))
        
        return TestResult(test_case.name, True, "All checks passed")

    def load_test_case(self, test_config_path):
        config_path = Path(test_config_path)

        if config_path.suffix == ".toml":
            if tomllib is None:
                raise RuntimeError(
                    "Python tomllib is unavailable. Use Python 3.11+ to run TOML-based tests."
                )
            with open(config_path, "rb") as handle:
                config = tomllib.load(handle)
            return self._build_toml_test_case(config_path, config)

        with open(config_path, "r") as handle:
            config = json.load(handle)
        return self._build_legacy_json_test_case(config_path, config)

    def _build_toml_test_case(self, config_path, config):
        metadata = dict(config.get("metadata", {}))
        if "tags" in config:
            metadata.setdefault("tags", config["tags"])
        if "description" in config:
            metadata.setdefault("description", config["description"])

        return TestCase(
            config_path=config_path,
            name=config.get("name", Path(config_path).parent.name),
            args=config.get("args", ["-genDocs"]),
            input_path=Path(config.get("input", "test.modt")),
            expected_outputs=config.get("outputs", []),
            timeout_seconds=config.get("timeout_seconds", 10),
            metadata=metadata,
            commands=config.get("commands", []),
            render_all_puml=config.get("render_all_puml", False),
            run_args=config.get("run_args"),
            run_cwd=config.get("run_cwd"),
        )

    def _build_legacy_json_test_case(self, config_path, config):
        return TestCase(
            config_path=config_path,
            name=config.get("name", str(config_path)),
            args=config.get("args", ["-genDocs"]),
            modt_content=config.get("modt_content", ""),
            expected_outputs=[self._normalize_legacy_output(output) for output in config.get("expected_outputs", [])],
            timeout_seconds=config.get("timeout_seconds", 10),
            metadata={"legacy": True},
            commands=config.get("commands", []),
            render_all_puml=config.get("render_all_puml", False),
            run_args=config.get("run_args"),
            run_cwd=config.get("run_cwd"),
        )

    def _normalize_legacy_output(self, output):
        normalized = dict(output)
        if "contains" in normalized:
            normalized["matches"] = normalized.pop("contains")
        if "not_contains" in normalized:
            normalized["not_matches"] = normalized.pop("not_contains")
        return normalized

    def prepare_input_file(self, test_case, test_dir):
        if test_case.input_path:
            source_path = (test_case.base_dir / test_case.input_path).resolve()
            target_path = test_dir / test_case.input_path.name

            if source_path.is_dir():
                shutil.copytree(source_path, target_path)
                return target_path

            target_path.write_text(source_path.read_text())
            return target_path

        target_path = test_dir / "test.modt"
        target_path.write_text(test_case.modt_content or "")
        return target_path

    def resolve_run_args(self, run_args, test_dir, modt_file):
        resolved_args = []
        replacements = {
            "{run_dir}": str(test_dir.resolve()),
            "{input_path}": str(modt_file.resolve()),
        }

        for arg in run_args:
            resolved = str(arg)
            for placeholder, value in replacements.items():
                resolved = resolved.replace(placeholder, value)
            resolved_args.append(resolved)

        return resolved_args

    def resolve_snapshot_path(self, test_case, expected):
        snapshot = expected.get("snapshot")
        if snapshot is None or snapshot is False:
            return None

        if snapshot is True:
            candidates = [
                test_case.base_dir / "expected" / expected["file"],
                test_case.base_dir / expected["file"],
            ]
        else:
            candidates = [test_case.base_dir / snapshot]

        for candidate in candidates:
            if candidate.exists():
                return candidate

        raise FileNotFoundError(
            f"Snapshot file for {expected['file']} not found near {test_case.config_path}"
        )

    def run_commands(self, test_case, test_dir):
        failures = []
        for command in test_case.commands:
            args = command.get("args") or command.get("run")
            if not args:
                failures.append(f"Command entry in {test_case.config_path} is missing 'args'")
                continue

            resolved_args = []
            for arg in args:
                resolved = str(arg).replace("{run_dir}", str(test_dir.resolve()))
                resolved_args.append(resolved)

            expected_exit = command.get("exit_code", 0)
            try:
                result = subprocess.run(
                    resolved_args,
                    capture_output=True,
                    text=True,
                    timeout=command.get("timeout_seconds", test_case.timeout_seconds),
                    cwd=test_dir,
                )
            except Exception as exc:
                failures.append(f"Command failed to execute {' '.join(resolved_args)}: {exc}")
                continue

            if result.returncode != expected_exit:
                details = "\n".join(part for part in [result.stdout.strip(), result.stderr.strip()] if part)
                failures.append(
                    f"Command {' '.join(resolved_args)} exited with {result.returncode}, expected {expected_exit}"
                    + (f"\n{details}" if details else "")
                )

        return failures

    def render_all_puml_outputs(self, test_dir):
        failures = []
        puml_files = sorted(test_dir.rglob("*.puml"))
        if not puml_files:
            failures.append("No .puml files were generated to render")
            return failures

        plantuml = shutil.which("plantuml")
        if plantuml is None:
            failures.append("plantuml is not available in PATH for render_all_puml checks")
            return failures

        for puml_file in puml_files:
            result = subprocess.run(
                [plantuml, "-tsvg", str(puml_file.resolve())],
                capture_output=True,
                text=True,
                cwd=test_dir,
            )
            if result.returncode != 0:
                details = "\n".join(part for part in [result.stdout.strip(), result.stderr.strip()] if part)
                failures.append(
                    f"PlantUML render failed for {puml_file.relative_to(test_dir)}"
                    + (f"\n{details}" if details else "")
                )

        return failures

def discover_tests():
    toml_tests = sorted(Path("Testing/temp").rglob("case.toml"))
    if toml_tests:
        return toml_tests
    return sorted(Path("Testing/tests").glob("*.json"))

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
