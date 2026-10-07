#!/usr/bin/env python3
"""Declare and verify the macOS version required by a staged app bundle."""

import argparse
import os
import plistlib
import re
import subprocess
from pathlib import Path


MACHO_MAGICS = {
    b"\xfe\xed\xfa\xce", b"\xce\xfa\xed\xfe",
    b"\xfe\xed\xfa\xcf", b"\xcf\xfa\xed\xfe",
    b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca",
    b"\xca\xfe\xba\xbf", b"\xbf\xba\xfe\xca",
}
VERSION = re.compile(r"^\s*(minos|version)\s+(\d+(?:\.\d+){0,2})\s*$")


def version_tuple(value):
    parts = tuple(int(part) for part in value.split("."))
    return (parts + (0, 0, 0))[:3]


def deployment_versions(otool_output):
    command = None
    versions = []
    for line in otool_output.splitlines():
        stripped = line.strip()
        if stripped.startswith("cmd "):
            command = stripped[4:]
            continue
        match = VERSION.match(line)
        if match and ((command == "LC_BUILD_VERSION" and match[1] == "minos")
                      or (command == "LC_VERSION_MIN_MACOSX" and match[1] == "version")):
            versions.append(version_tuple(match[2]))
    return versions


def macho_files(contents):
    for root, _dirs, names in os.walk(contents):
        for name in names:
            path = Path(root, name)
            if path.is_symlink():
                continue
            with path.open("rb") as binary:
                if binary.read(4) in MACHO_MAGICS:
                    yield path


def required_version(contents):
    required = (0, 0, 0)
    count = 0
    for path in macho_files(contents):
        result = subprocess.run(["otool", "-l", str(path)], capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(f"otool failed for {path}: {result.stderr.strip()}")
        versions = deployment_versions(result.stdout)
        if not versions:
            raise RuntimeError(f"No macOS deployment version found in {path}")
        required = max(required, *versions)
        count += 1
    if not count:
        raise RuntimeError(f"No Mach-O binaries found in {contents}")
    return required, count


def display_version(version):
    return ".".join(map(str, version))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("app", type=Path, help="Staged Apscribe.app")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true", help="Set LSMinimumSystemVersion before signing")
    mode.add_argument("--check", action="store_true", help="Fail if the declared version is too low")
    args = parser.parse_args()

    plist_path = args.app / "Contents" / "Info.plist"
    with plist_path.open("rb") as stream:
        info = plistlib.load(stream)
    required, count = required_version(args.app / "Contents")
    if args.write:
        info["LSMinimumSystemVersion"] = display_version(required)
        with plist_path.open("wb") as stream:
            plistlib.dump(info, stream)
    else:
        declared = info.get("LSMinimumSystemVersion")
        if not isinstance(declared, str) or version_tuple(declared) < required:
            raise SystemExit(
                f"Bundle declares macOS {declared or 'unspecified'}, but bundled binaries require "
                f"macOS {display_version(required)}"
            )
    print(f"Checked {count} Mach-O binaries; bundle requires macOS {display_version(required)}")


if __name__ == "__main__":
    main()
