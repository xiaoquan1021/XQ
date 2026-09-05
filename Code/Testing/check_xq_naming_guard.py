#!/usr/bin/env python3
"""XQ naming guard: scan XQ/Code for forbidden SV residuals."""

import os
import re
import sys

SCAN_ROOT = "/home/xiaoquan/XQ/Code"
SKIP_DIRS = {"build"}
BINARY_EXTENSIONS = {
    ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".ico", ".svg",
    ".ttf", ".otf", ".woff", ".woff2",
    ".so", ".a", ".o", ".obj", ".lib", ".dll", ".dylib",
    ".exe", ".bin", ".dat", ".pyc", ".pyo",
    ".zip", ".tar", ".gz", ".bz2", ".xz", ".7z",
    ".pdf", ".doc", ".docx",
}
FORBIDDEN_PATTERNS = [
    "svExternals",
    "SV Project Detected",
    "This directory appears to be an SV project",
    "XQ can import SV projects",
    "Importing SV project",
    "Failed to import SV project",
    "Successfully imported SV project",
    "SVProjectImporter",
    "ImportedSVProject",
    "SimVascular-style",
    "sv4gui",
    "org.sv",
]

FORBIDDEN_FILENAME_PATTERN = re.compile(r"sv.*\.png$", re.IGNORECASE)

def is_binary(filename):
    ext = os.path.splitext(filename)[1].lower()
    return ext in BINARY_EXTENSIONS

def main():
    self_path = os.path.abspath(__file__)
    violations = []
    for dirpath, dirnames, filenames in os.walk(SCAN_ROOT):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for fname in filenames:
            fpath = os.path.join(dirpath, fname)
            if os.path.abspath(fpath) == self_path:
                continue
            if ".backup_" in fname:
                continue
            if FORBIDDEN_FILENAME_PATTERN.match(fname):
                violations.append((fpath, 0, f"forbidden filename: {fname}"))
                continue
            if is_binary(fname):
                continue
            try:
                with open(fpath, "r", errors="ignore") as f:
                    for lineno, line in enumerate(f, 1):
                        for pat in FORBIDDEN_PATTERNS:
                            if pat in line:
                                violations.append(
                                    (fpath, lineno, f"forbidden pattern: {pat!r} in: {line.strip()[:200]}")
                                )
            except Exception:
                pass

    if violations:
        for fpath, lineno, msg in violations:
            print(f"{fpath}:{lineno}: {msg}")
        print(f"XQ_NAMING_GUARD_FAILED: {len(violations)} violation(s)")
        sys.exit(1)

    print("XQ_NAMING_GUARD_OK")

if __name__ == "__main__":
    main()
