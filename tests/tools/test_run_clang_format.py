# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors
"""Tests for tools/run-clang-format.py."""
from __future__ import annotations

import runpy
from pathlib import Path

import pytest
from script_loader import TOOLS_DIR, load_script

rcf = load_script("run-clang-format.py")


def _sources(tmp_path: Path) -> Path:
    (tmp_path / "src").mkdir()
    (tmp_path / "tests").mkdir()
    (tmp_path / "src" / "a.cpp").write_text("x\n", encoding="utf-8")
    (tmp_path / "src" / "b.hpp").write_text("x\n", encoding="utf-8")
    (tmp_path / "src" / "readme.md").write_text("x\n", encoding="utf-8")  # not a C/C++ source
    # No 'tools' dir -> exercises the `base.is_dir()` skip branch.
    return tmp_path


def _fake_run(captured: dict, returncode: int):
    def run(cmd, cwd=None, check=False):
        captured["cmd"] = cmd
        captured["cwd"] = cwd
        return type("R", (), {"returncode": returncode})()

    return run


def test_discover_sources_sorted_and_filtered(tmp_path, monkeypatch):
    monkeypatch.setattr(rcf, "REPO_ROOT", _sources(tmp_path))
    assert [p.name for p in rcf.discover_sources()] == ["a.cpp", "b.hpp"]


def test_main_check_builds_dry_run(tmp_path, monkeypatch):
    monkeypatch.setattr(rcf, "REPO_ROOT", _sources(tmp_path))
    monkeypatch.setattr("sys.argv", ["run-clang-format.py", "--check"])
    captured: dict = {}
    monkeypatch.setattr("subprocess.run", _fake_run(captured, 0))
    assert rcf.main() == 0
    assert "--dry-run" in captured["cmd"] and "--Werror" in captured["cmd"]


def test_main_in_place_passes_through_return_code(tmp_path, monkeypatch):
    monkeypatch.setattr(rcf, "REPO_ROOT", _sources(tmp_path))
    monkeypatch.setattr("sys.argv", ["run-clang-format.py"])
    captured: dict = {}
    monkeypatch.setattr("subprocess.run", _fake_run(captured, 3))
    assert rcf.main() == 3  # the clang-format exit code is returned verbatim
    assert "-i" in captured["cmd"] and "--dry-run" not in captured["cmd"]


def test_main_no_sources(tmp_path, monkeypatch, capsys):
    monkeypatch.setattr(rcf, "REPO_ROOT", tmp_path)  # empty tree
    monkeypatch.setattr("sys.argv", ["run-clang-format.py"])
    assert rcf.main() == 0
    assert "No sources found." in capsys.readouterr().out


def test_main_clang_format_missing(tmp_path, monkeypatch, capsys):
    monkeypatch.setattr(rcf, "REPO_ROOT", _sources(tmp_path))
    monkeypatch.setattr("sys.argv", ["run-clang-format.py"])

    def boom(cmd, cwd=None, check=False):
        raise FileNotFoundError

    monkeypatch.setattr("subprocess.run", boom)
    assert rcf.main() == 2
    assert "not found on PATH" in capsys.readouterr().err


def test_entry_guard_runs_as_main(monkeypatch):
    # Execute the script as __main__ so the `raise SystemExit(main())` guard runs.
    # Stub discovery and subprocess so the test never walks the tree or spawns a real
    # process: a faulted subprocess.run -> FileNotFoundError -> exit 2.
    def _missing(cmd, cwd=None, check=False):
        raise FileNotFoundError

    monkeypatch.setattr(Path, "rglob", lambda self, pattern: iter([Path("x.cpp")]))
    monkeypatch.setattr("subprocess.run", _missing)
    monkeypatch.setattr("sys.argv", ["run-clang-format.py", "--check"])
    with pytest.raises(SystemExit) as exc:
        runpy.run_path(str(TOOLS_DIR / "run-clang-format.py"), run_name="__main__")
    assert exc.value.code == 2
