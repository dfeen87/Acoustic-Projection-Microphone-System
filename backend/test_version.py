"""Current release metadata must agree across runtime and delivery layers."""
import json
import re
from pathlib import Path

from backend.app import app


def test_current_version_is_consistent():
    root = Path(__file__).resolve().parents[1]
    version = re.search(r"project\(AcousticProjectionMicrophone VERSION (\d+\.\d+\.\d+)", (root / "CMakeLists.txt").read_text()).group(1)
    assert app.version == version
    for package in ("ui", "launcher"):
        manifest = json.loads((root / package / "package.json").read_text())
        lock = json.loads((root / package / "package-lock.json").read_text())
        assert manifest["version"] == lock["version"] == lock["packages"][""]["version"] == version
    assert re.search(r'^#define MyAppVersion "([^"]+)"', (root / "installers/setup.iss").read_text(), re.M).group(1) == version
    assert re.search(r"^version: (.+)$", (root / "CITATION.cff").read_text(), re.M).group(1) == version
