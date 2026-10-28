#!/usr/bin/env python3
"""Build, check, package and upload Mystic Balloon using the external CHGame SDK."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import venv

ROOT = Path(__file__).resolve().parent
SDK = ROOT / "CHGame"
SDK_URL = "https://github.com/bateske/CHGame.git"
SDK_COMMIT = "e1f8d05940ddb7bbef89c13326da75c003f02904"
BOARD_VERSION = "0.3.0"
PYTHON = ROOT / ".venv" / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
env = dict(os.environ)
for key, path in {
    "ARDUINO_CONFIG_FILE": ROOT / "arduino-cli.yaml",
    "ARDUINO_DIRECTORIES_DATA": ROOT / ".arduino15",
    "ARDUINO_DIRECTORIES_DOWNLOADS": ROOT / ".arduino15/staging",
    "ARDUINO_DIRECTORIES_USER": ROOT / "sketchbook",
    "ARDUINO_BUILD_CACHE_PATH": ROOT / ".arduino15/cache",
}.items():
    env.setdefault(key, str(path))


def run(*args):
    return subprocess.run([str(a) for a in args], cwd=ROOT, env=env, check=True)


def setup(host_only):
    if sys.version_info < (3, 12):
        raise SystemExit("Setup requires Python 3.12 or later.")
    required = ["git"] + ([] if host_only else ["arduino-cli"])
    for program in required:
        if not shutil.which(program):
            raise SystemExit(f"Install {program} and add it to PATH before setup.")
    if not SDK.exists():
        run("git", "clone", SDK_URL, SDK)
        run("git", "-C", SDK, "checkout", "--detach", SDK_COMMIT)
    actual = subprocess.check_output(["git", "-C", str(SDK), "rev-parse", "HEAD"], text=True).strip()
    if actual != SDK_COMMIT:
        raise SystemExit(f"Expected CHGame SDK {SDK_COMMIT}; found {actual}. Existing SDK was left in place.")
    if not PYTHON.exists():
        venv.EnvBuilder(with_pip=True).create(ROOT / ".venv")
    run(PYTHON, "-m", "pip", "install", "-r", ROOT / "requirements.txt")
    if not host_only:
        run("arduino-cli", "core", "update-index")
        run("arduino-cli", "core", "install", f"CHGame:ch32v@{BOARD_VERSION}")


def main():
    args = sys.argv[1:] or ["build"]
    if args[0] == "setup":
        if args[1:] not in ([], ["--host-only"]):
            raise SystemExit("usage: python3 mysticballoon.py setup [--host-only]")
        setup("--host-only" in args)
        return 0
    if not (SDK / "tools/chgame.py").is_file():
        raise SystemExit("Run python3.12 mysticballoon.py setup first (or setup --host-only for simulator checks).")
    python = Path(env.get("CHGAME_TOOL_PYTHON", PYTHON))
    if not python.is_file():
        raise SystemExit("Python tools are missing: run python3.12 mysticballoon.py setup.")
    if args[:2] == ["cart", "deploy"]:
        for i, value in enumerate(args):
            card = args[i+1] if value == "--card" and i+1 < len(args) else value[7:] if value.startswith("--card=") else None
            if card:
                run(python, ROOT / "MysticBalloon/tools/prepare_saves.py", "--card", card)
    return subprocess.call([str(python), str(SDK / "tools/chgame.py"),
                            "--sketch", str(ROOT / "MysticBalloon"), *args], cwd=ROOT, env=env)


if __name__ == "__main__":
    raise SystemExit(main())
