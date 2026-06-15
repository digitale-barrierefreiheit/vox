# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors
"""Tests for tools/check-license-headers.py."""
from __future__ import annotations

import runpy
from pathlib import Path

import pytest
from conftest import TOOLS_DIR, load_script

clh = load_script("check-license-headers.py")

HEADER = "// SPDX-License-Identifier: Apache-2.0\n"


def test_requires_header_by_suffix_and_name():
    assert clh.requires_header(Path("a/b/foo.cpp"))
    assert clh.requires_header(Path("x/CMakeLists.txt"))
    assert clh.requires_header(Path("t/config.hpp.in"))  # configure_file template
    assert clh.requires_header(Path("s/script.py"))
    assert not clh.requires_header(Path("d/readme.md"))
    assert not clh.requires_header(Path("d/data.json"))


def test_is_excluded(tmp_path, monkeypatch):
    monkeypatch.setattr(clh, "REPO_ROOT", tmp_path)
    assert clh.is_excluded(tmp_path / "build" / "x.cpp")
    assert clh.is_excluded(tmp_path / "build")  # the excluded dir itself
    assert clh.is_excluded(tmp_path / "doc" / "reference" / "r.hpp")
    assert not clh.is_excluded(tmp_path / "src" / "x.cpp")


def test_has_header(tmp_path):
    good = tmp_path / "good.cpp"
    good.write_text("#!/bin/sh\n" + HEADER, encoding="utf-8")  # shebang before the tag
    assert clh.has_header(good)

    missing = tmp_path / "bad.cpp"
    missing.write_text("int main() {}\n", encoding="utf-8")
    assert not clh.has_header(missing)

    late = tmp_path / "late.cpp"
    late.write_text("\n" * 10 + HEADER, encoding="utf-8")  # tag beyond the scan window
    assert not clh.has_header(late)

    binary = tmp_path / "b.cpp"
    binary.write_bytes(b"\xff\xfe\x00\x01")  # invalid UTF-8 -> UnicodeDecodeError caught
    assert not clh.has_header(binary)

    assert not clh.has_header(tmp_path / "nope.cpp")  # missing file -> OSError caught


def _tree(tmp_path: Path) -> Path:
    (tmp_path / "src").mkdir()
    (tmp_path / "build").mkdir()
    (tmp_path / "src" / "a.cpp").write_text(HEADER, encoding="utf-8")
    (tmp_path / "src" / "b.hpp").write_text("no header\n", encoding="utf-8")
    (tmp_path / "src" / "readme.md").write_text("x\n", encoding="utf-8")  # not a source
    (tmp_path / "build" / "gen.cpp").write_text("no header\n", encoding="utf-8")  # excluded
    return tmp_path


def test_discover_filters_excluded_and_nonsource(tmp_path, monkeypatch):
    monkeypatch.setattr(clh, "REPO_ROOT", _tree(tmp_path))
    assert {p.name for p in clh.discover()} == {"a.cpp", "b.hpp"}


def test_main_list(tmp_path, monkeypatch, capsys):
    monkeypatch.setattr(clh, "REPO_ROOT", _tree(tmp_path))
    monkeypatch.setattr("sys.argv", ["check-license-headers.py", "--list"])
    assert clh.main() == 0
    out = capsys.readouterr().out
    assert "src/a.cpp" in out and "src/b.hpp" in out


def test_main_reports_missing(tmp_path, monkeypatch, capsys):
    monkeypatch.setattr(clh, "REPO_ROOT", _tree(tmp_path))
    monkeypatch.setattr("sys.argv", ["check-license-headers.py"])
    assert clh.main() == 1
    err = capsys.readouterr().err
    assert "Missing SPDX" in err and "src/b.hpp" in err


def test_main_all_present(tmp_path, monkeypatch, capsys):
    (tmp_path / "src").mkdir()
    (tmp_path / "src" / "a.cpp").write_text(HEADER, encoding="utf-8")
    monkeypatch.setattr(clh, "REPO_ROOT", tmp_path)
    monkeypatch.setattr("sys.argv", ["check-license-headers.py"])
    assert clh.main() == 0
    assert "OK:" in capsys.readouterr().out


def test_entry_guard_runs_as_main(monkeypatch):
    # Execute the script as __main__ so the `raise SystemExit(main())` guard runs.
    # Stub the directory walk to one real file (discovery itself is tested above) so
    # this stays a fast, hermetic entry-point smoke test instead of scanning the repo.
    import pathlib

    script = TOOLS_DIR / "check-license-headers.py"
    monkeypatch.setattr(pathlib.Path, "rglob", lambda self, pattern: iter([script]))
    monkeypatch.setattr("sys.argv", ["check-license-headers.py", "--list"])
    with pytest.raises(SystemExit) as exc:
        runpy.run_path(str(script), run_name="__main__")
    assert exc.value.code == 0
