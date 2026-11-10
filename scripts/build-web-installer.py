#!/usr/bin/env python3
"""Build the FM1-Lab Web MIDI installer from fm1-base's maintained installer.

Usage: python3 scripts/build-web-installer.py PACKAGE.fwsc OUTPUT_DIR
"""
from __future__ import annotations

import hashlib
import json
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "base"
BLOCKS, BLK, KEEP = 20, 0x30, 0x2F
LOADER_MARK = b"FELUCCA-LOADER-1"
PRODUCT_PREFIX = "FM-1_97"
VERSION = "0.1"
NAME = "Shipwreck"
REPO = "https://github.com/albinoIguana/FM1-Lab"
CREDITS = " An independent FM1-Lab firmware build; preserve the fm1-base, Felucca and M-VAVE notices."

def strip_module(src: str) -> str:
    src = re.sub(r"^export\s+", "", src, flags=re.M)
    return re.sub(r"^import .*?;\n", "", src, flags=re.M)

def product_of(raw: bytes) -> str:
    if len(raw) < BLOCKS * BLK:
        raise SystemExit("Package is too short to be an FM-1 .fwsc file")
    return "".join(chr((marker - i - 1) & 0xFF)
                   for i in range(BLOCKS)
                   if (marker := raw[i * BLK + KEEP]) != 0x7D)

def main(package: Path, out: Path) -> None:
    raw = package.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(re.escape(PRODUCT_PREFIX) + r"\d{6}", product):
        raise SystemExit(f"Unexpected package identity {product!r}; expected {PRODUCT_PREFIX} plus six release digits")
    if LOADER_MARK not in raw:
        raise SystemExit("Package does not contain the Felucca update loader; refusing to publish")

    web = BASE / "web"
    for filename in ("fm1ports.js", "fm1pkg.js", "fm1ota.js"):
        if not (web / filename).is_file():
            raise SystemExit(f"Missing pinned fm1-base installer component: {web / filename}")

    out.mkdir(parents=True, exist_ok=True)
    firmware = out / "firmware"
    firmware.mkdir(parents=True, exist_ok=True)
    pkg_name = f"fm1lab-{VERSION}.fwsc"
    shutil.copyfile(package, firmware / pkg_name)

    html = (web / "installer.html").read_text(encoding="utf-8")
    library = "\n".join(
        strip_module((web / filename).read_text(encoding="utf-8"))
        for filename in ("fm1ports.js", "fm1pkg.js", "fm1ota.js")
    )
    metadata = json.dumps({
        "version": VERSION,
        "product": product,
        "pkg": "firmware/" + pkg_name,
        "backup": False,
        "res": None,
        "boot": None,
    })
    for marker in ("/*LIB*/", "/*META*/", "/*APP*/"):
        if html.count(marker) != 1:
            raise SystemExit(f"Expected exactly one {marker} placeholder in fm1-base installer")
    html = html.replace("{{NAME}}", NAME).replace("{{REPO}}", REPO)
    html = html.replace("{{REPO_SHORT}}", REPO.removeprefix("https://"))
    html = html.replace("{{LICENSE}}", "GPL-3.0-only")
    html = html.replace("{{CREDITS}}", CREDITS)
    html = html.replace("{{RES}}", "firmware resources").replace("{{RES_JA}}", "firmware resources")
    html = html.replace("/*LIB*/", library).replace("/*META*/", metadata).replace("/*APP*/", "null")

    # The fm1-base page's normal acknowledgement remains mandatory. Add a clear
    # preflight warning because this app has no app-level backup endpoint:
    # users must save a full-flash backup before the first install.
    notice = """
  <aside class="backup" style="margin: 0 0 1rem">
    <strong>Before installing: make a full-flash backup.</strong>
    This installer does not back up the FM1. Use
    <a href="https://github.com/jvitkauskas/m-flasher/releases" target="_blank" rel="noopener">M-Flasher for macOS</a>
    to save the complete 1 MiB flash image first. Keep that file somewhere safe.
    Do not continue until the backup is saved.
  </aside>
"""
    html = html.replace('<h1>Install ' + NAME + '</h1>', '<h1>Install ' + NAME + '</h1>' + notice)
    (out / "index.html").write_text(html, encoding="utf-8")
    (out / "package-info.json").write_text(json.dumps({
        "version": VERSION,
        "product": product,
        "filename": pkg_name,
        "bytes": len(raw),
        "sha256": hashlib.sha256(raw).hexdigest(),
        "loader": "FELUCCA-LOADER-1",
        "installer_source": "fm1-base (pinned submodule)",
        "mbootloader_offered": False,
    }, indent=2) + "\n", encoding="utf-8")
    (out / ".nojekyll").write_text("", encoding="utf-8")
    print(f"Built Web MIDI installer: {out / 'index.html'}")
    print(f"Package: {pkg_name}; identity {product}; {len(raw)} bytes")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    main(Path(sys.argv[1]), Path(sys.argv[2]))
