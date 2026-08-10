#!/usr/bin/env python3
"""Check local Markdown links in .okf and repair OKF-root links for GitHub.

OKF permits bundle-root links such as /fixes/example.md, but GitHub interprets
that destination relative to github.com rather than the repository's .okf/
directory. This project therefore publishes document-relative links.
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from pathlib import Path
from urllib.parse import unquote, urlsplit

LINK_RE = re.compile(r"(?P<prefix>!?\[[^\]]*\]\()(?P<destination><[^>]+>|[^\s)]+)(?P<suffix>[^)]*\))")
EXTERNAL_SCHEMES = {"http", "https", "mailto", "tel", "data"}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "root",
        nargs="?",
        default=".",
        help="repository root containing .okf (default: current directory)",
    )
    parser.add_argument(
        "--fix-root-links",
        action="store_true",
        help="rewrite /concept links to document-relative GitHub-safe links",
    )
    return parser.parse_args()


def split_destination(raw: str) -> tuple[str, str]:
    destination = raw[1:-1] if raw.startswith("<") and raw.endswith(">") else raw
    if "#" not in destination:
        return destination, ""
    path, fragment = destination.split("#", 1)
    return path, f"#{fragment}"


def relative_destination(source: Path, target: Path, fragment: str) -> str:
    relative = Path(os.path.relpath(target, source.parent)).as_posix()
    if target.is_dir() and not relative.endswith("/"):
        relative += "/"
    return relative + fragment


def main() -> int:
    args = parse_args()
    root = Path(args.root).resolve()
    okf = root / ".okf"
    if not okf.is_dir():
        print(f"ERROR: OKF bundle not found: {okf}")
        return 1

    errors: list[str] = []
    rewrites = 0

    for document in sorted(okf.rglob("*.md")):
        original = document.read_text(encoding="utf-8")
        lines = original.splitlines(keepends=True)
        output: list[str] = []
        fence_marker: str | None = None

        for line_number, line in enumerate(lines, 1):
            stripped = line.lstrip()
            marker = stripped[:3]
            if marker in {"```", "~~~"}:
                if fence_marker is None:
                    fence_marker = marker
                elif fence_marker == marker:
                    fence_marker = None
                output.append(line)
                continue
            if fence_marker is not None:
                output.append(line)
                continue

            def replace(match: re.Match[str]) -> str:
                nonlocal rewrites
                raw = match.group("destination")
                path_text, fragment = split_destination(raw)
                parsed = urlsplit(path_text)
                if parsed.scheme.lower() in EXTERNAL_SCHEMES or path_text.startswith("#"):
                    return match.group(0)
                if not path_text:
                    return match.group(0)

                decoded = unquote(path_text)
                if decoded.startswith("/"):
                    bundle_target = okf / decoded.lstrip("/")
                    repository_target = root / decoded.lstrip("/")
                    target = bundle_target if bundle_target.exists() else repository_target
                    if args.fix_root_links and target.exists():
                        replacement = relative_destination(document, target.resolve(), fragment)
                        rewrites += 1
                        return match.group("prefix") + replacement + match.group("suffix")
                    errors.append(
                        f"{document.relative_to(root)}:{line_number}: "
                        f"GitHub-unsafe root link {path_text!r}"
                    )
                    return match.group(0)

                target = (document.parent / decoded).resolve()
                try:
                    target.relative_to(root)
                except ValueError:
                    errors.append(
                        f"{document.relative_to(root)}:{line_number}: "
                        f"link escapes repository: {path_text!r}"
                    )
                    return match.group(0)

                if not target.exists():
                    errors.append(
                        f"{document.relative_to(root)}:{line_number}: "
                        f"missing local target {path_text!r}"
                    )
                return match.group(0)

            output.append(LINK_RE.sub(replace, line))

        updated = "".join(output)
        if updated != original:
            document.write_text(updated, encoding="utf-8")

    if rewrites:
        print(f"Rewrote {rewrites} GitHub-unsafe root link(s).")
    if errors:
        print("OKF link check failed:")
        for error in errors:
            print(f"  - {error}")
        if not args.fix_root_links:
            print("Run again with --fix-root-links to repair resolvable /... links.")
        return 1

    print("OKF link check passed: all local links resolve and are GitHub-safe.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
