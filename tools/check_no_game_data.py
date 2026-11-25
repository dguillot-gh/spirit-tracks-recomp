#!/usr/bin/env python3
"""Copyright guard: fail if game-derived data is about to be committed.

Checks every tracked file (default), only staged files (--staged), or explicit paths.
See docs/LEGAL.md for the policy this enforces.

  python tools/check_no_game_data.py               # scan all tracked files (CI)
  python tools/check_no_game_data.py --staged      # scan staged files (pre-commit)
  python tools/check_no_game_data.py --install-hook
"""
from __future__ import annotations

import argparse
import hashlib
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

# Extensions that are never allowed in the repo.
FORBIDDEN_EXTS = {
    ".nds", ".srl", ".dsi", ".ids", ".sav", ".dsv",
    ".rsb", ".mods", ".sdat", ".sseq", ".sbnk", ".swar", ".strm",
    ".nsbmd", ".nsbtx", ".nsbca", ".nsbta", ".nsbtp", ".nsbma", ".narc",
    ".iso", ".wbfs", ".rvz", ".wad",
    ".thp",
}

# Magic numbers at offset 0 of DS game file formats.
FORBIDDEN_MAGICS = [
    b"MODS", b"SDAT", b"SSEQ", b"SBNK", b"SWAR", b"STRM", b"NARC",
    b"BMD0", b"BTX0", b"BCA0", b"BTA0", b"BTP0", b"BMA0", b"BVA0",
    b"RGCN", b"RLCN", b"RCSN", b"RECN", b"RNAN",
    b"THP\0", b"WBFS",  # (videos, disc images)
]

# SHA-1 of known game dumps (verification facts, not data).
KNOWN_ROM_SHA1 = {
    "1f0921b5e99db9d92d24710f1b004e4f3c997b73",  # Spirit Tracks (USA, Australia)
    "6a63d88702c34da626e273dfca4931d52ac4551b",  # Spirit Tracks (USA), D-pad patch v0.9c
    "6896c1db2b3a725cb991f711a875fc2228b7620e",  # Spirit Tracks (USA), D-pad + QoL patch v0.9c
    "9e99cc803a14ce038eb908db585431f8254f09ee",  # Spirit Tracks (Europe)
    "dcb221a89acb894503a461c2c7d4cfdad9a06b68",  # Spirit Tracks (Europe) (Rev 1), the dumps zeldaret/st lists
    "b9096f46e9163750a9e3f6c2fc0758e6094b6135",
    "b0288a880498c12508e53f4373085eb88955e14b",
    "4efbff9a263fe622f3f7e7cfb7bea8046b86e57d",
    "52d7f22255d5d86fc40d3a43c7de969e0365ff7d",
}

# Marker the recompiler writes at the top of every generated file.
GENERATED_MARKER = b"@ntr-recomp-generated"

MAX_BINARY_SIZE = 1 * 1024 * 1024  # larger binaries need an explicit allowlist entry
ALLOWLIST_FILE = REPO / "tools" / "game_data_allowlist.txt"


def git(*args: str) -> list[str]:
    out = subprocess.run(["git", *args], cwd=REPO, check=True, capture_output=True, text=True).stdout
    return [line for line in out.splitlines() if line]


def is_binary(data: bytes) -> bool:
    if b"\0" in data[:8192]:
        return True
    try:
        data[:8192].decode("utf-8")
        return False
    except UnicodeDecodeError:
        return True


def looks_like_nds_header(data: bytes) -> bool:
    # NDS header: ARM9 ROM offset at 0x20 is normally 0x4000, and the Nintendo logo CRC at 0x15C is 0xCF56.
    return len(data) >= 0x200 and data[0x15C:0x15E] == b"\x56\xCF"


def load_allowlist() -> set[str]:
    if not ALLOWLIST_FILE.exists():
        return set()
    return {
        line.strip() for line in ALLOWLIST_FILE.read_text().splitlines()
        if line.strip() and not line.startswith("#")
    }


def check_file(rel: str, data: bytes, allow: set[str]) -> list[str]:
    problems = []
    p = Path(rel)
    if p.suffix.lower() in FORBIDDEN_EXTS:
        problems.append(f"forbidden file type '{p.suffix}'")
    if hashlib.sha1(data).hexdigest() in KNOWN_ROM_SHA1:
        problems.append("file is a known game ROM")
    if is_binary(data):
        if data[:4] in FORBIDDEN_MAGICS:
            problems.append(f"DS game data signature {data[:4]!r}")
        if looks_like_nds_header(data):
            problems.append("contains an NDS cartridge header")
        if b"[SDK+NINTENDO" in data:
            problems.append("contains NitroSDK build tags (dumped game code?)")
        if len(data) > MAX_BINARY_SIZE and rel not in allow:
            problems.append(f"binary larger than {MAX_BINARY_SIZE // 1024} KiB (add to allowlist only if it is our own asset)")
    if GENERATED_MARKER in data[:4096] and rel != "tools/check_no_game_data.py":
        problems.append("recompiler-generated code (must stay under build/)")
    return problems


def staged_blob(rel: str) -> bytes:
    return subprocess.run(["git", "show", f":{rel}"], cwd=REPO, check=True, capture_output=True).stdout


def install_hook() -> None:
    hook = REPO / ".git" / "hooks" / "pre-commit"
    hook.write_text("#!/bin/sh\nexec python tools/check_no_game_data.py --staged\n")
    hook.chmod(0o755)
    print(f"installed {hook}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--staged", action="store_true", help="scan the staged versions of staged files")
    ap.add_argument("--install-hook", action="store_true", help="install as a git pre-commit hook")
    ap.add_argument("paths", nargs="*", help="explicit files to scan")
    args = ap.parse_args()

    if args.install_hook:
        install_hook()
        return 0

    allow = load_allowlist()
    if args.paths:
        files = [(p, Path(p).read_bytes()) for p in args.paths]
    elif args.staged:
        # Submodules (gitlinks, mode 160000) are pinned upstream sources, not blobs; skip them.
        gitlinks = {line.split("\t", 1)[1] for line in git("ls-files", "--stage") if line.startswith("160000 ")}
        files = [(p, staged_blob(p)) for p in git("diff", "--cached", "--name-only", "--diff-filter=ACMR")
                 if p not in gitlinks]
    else:
        files = [(p, (REPO / p).read_bytes()) for p in git("ls-files") if (REPO / p).is_file()]

    failed = False
    for rel, data in files:
        for problem in check_file(rel, data, allow):
            print(f"BLOCKED {rel}: {problem}", file=sys.stderr)
            failed = True

    if failed:
        print("\nGame-derived data must never be committed. See docs/LEGAL.md.", file=sys.stderr)
        return 1
    print(f"copyright guard: {len(files)} file(s) clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
