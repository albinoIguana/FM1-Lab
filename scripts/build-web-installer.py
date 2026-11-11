#!/usr/bin/env python3
"""Build the FM1-Lab Web MIDI installer from fm1-base's maintained installer.

Usage: python3 scripts/build-web-installer.py PACKAGE.fwsc OUTPUT_DIR
"""
from __future__ import annotations

import hashlib
import html
import json
import os
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

    build_number = os.environ.get("FM1_BUILD_NUMBER", "local")
    run_id = os.environ.get("FM1_BUILD_RUN_ID", "")
    build_sha = os.environ.get("FM1_BUILD_SHA", "unknown")
    build_date = os.environ.get("FM1_BUILD_DATE", "not recorded")
    run_url = f"{REPO}/actions/runs/{run_id}" if run_id else f"{REPO}/actions"

    web = BASE / "web"
    for filename in ("fm1ports.js", "fm1pkg.js", "fm1ota.js"):
        if not (web / filename).is_file():
            raise SystemExit(f"Missing pinned fm1-base installer component: {web / filename}")

    out.mkdir(parents=True, exist_ok=True)
    firmware = out / "firmware"
    firmware.mkdir(parents=True, exist_ok=True)
    pkg_name = f"fm1lab-{VERSION}.fwsc"
    shutil.copyfile(package, firmware / pkg_name)
    package_sha256 = hashlib.sha256(raw).hexdigest()

    html_doc = (web / "installer.html").read_text(encoding="utf-8")
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
        if html_doc.count(marker) != 1:
            raise SystemExit(f"Expected exactly one {marker} placeholder in fm1-base installer")
    html_doc = html_doc.replace("{{NAME}}", NAME).replace("{{REPO}}", REPO)
    html_doc = html_doc.replace("{{REPO_SHORT}}", REPO.removeprefix("https://"))
    html_doc = html_doc.replace("{{LICENSE}}", "GPL-3.0-only")
    html_doc = html_doc.replace("{{CREDITS}}", CREDITS)
    html_doc = html_doc.replace("{{RES}}", "firmware resources").replace("{{RES_JA}}", "firmware resources")
    html_doc = html_doc.replace("/*LIB*/", library).replace("/*META*/", metadata).replace("/*APP*/", "null")

    # Build identity is derived from the workflow run that produced this exact
    # package artifact, never from the later Pages checkout commit.
    build_panel = f"""
  <section aria-label="Firmware build information" style="margin: 1rem 0; padding: 1rem; border: 1px solid #59616b; border-radius: 12px; background: rgba(127,127,127,.08); overflow-wrap: anywhere">
    <h2 style="margin: 0 0 .65rem; font-size: 1.05rem">Firmware build information</h2>
    <dl style="display: grid; grid-template-columns: max-content minmax(0,1fr); gap: .35rem .8rem; margin: 0; font-size: .9rem">
      <dt>Firmware</dt><dd style="margin: 0">{html.escape(VERSION)} (Shipwreck)</dd>
      <dt>Build number</dt><dd style="margin: 0">#{html.escape(build_number)}</dd>
      <dt>Commit</dt><dd style="margin: 0"><code>{html.escape(build_sha[:8])}</code></dd>
      <dt>Built</dt><dd style="margin: 0">{html.escape(build_date)} UTC</dd>
      <dt>Package size</dt><dd style="margin: 0">{len(raw):,} bytes</dd>
      <dt>SHA-256</dt><dd style="margin: 0"><code style="font-size: .78rem">{package_sha256}</code></dd>
      <dt>Build run</dt><dd style="margin: 0"><a href="{html.escape(run_url, quote=True)}" target="_blank" rel="noopener">View build and test results</a></dd>
    </dl>
    <p style="margin: .75rem 0 0; font-size: .82rem">Check the build number and commit before installing. The checksum identifies the exact firmware package served by this page.</p>
  </section>
"""
    heading = '<h1>Install ' + NAME + '</h1>'
    if html_doc.count(heading) != 1:
        raise SystemExit("Could not find the expected installer heading to place build information")
    html_doc = html_doc.replace(heading, heading + build_panel)

    # This installer has no app-level backup endpoint: require a full-flash
    # backup before installation.
    notice = """
  <aside class="backup" style="margin: 0 0 1rem">
    <strong>Before installing: make a full-flash backup.</strong>
    This installer does not back up the FM1. Use
    <a href="https://github.com/jvitkauskas/m-flasher/releases" target="_blank" rel="noopener">M-Flasher for macOS</a>
    to save the complete 1 MiB flash image first. Keep that file somewhere safe.
    Do not continue until the backup is saved.
  </aside>
"""
    html_doc = html_doc.replace(heading + build_panel, heading + notice + build_panel)
    (out / "index.html").write_text(html_doc, encoding="utf-8")
    (out / "package-info.json").write_text(json.dumps({
        "version": VERSION,
        "product": product,
        "filename": pkg_name,
        "bytes": len(raw),
        "sha256": package_sha256,
        "build_number": build_number,
        "build_run_id": run_id or None,
        "build_run_url": run_url,
        "build_sha": build_sha,
        "build_date_utc": build_date,
        "loader": "FELUCCA-LOADER-1",
        "installer_source": "fm1-base (pinned submodule)",
        "mbootloader_offered": False,
    }, indent=2) + "\n", encoding="utf-8")
    (out / ".nojekyll").write_text("", encoding="utf-8")
    print(f"Built Web MIDI installer: {out / 'index.html'}")
    print(f"Package: {pkg_name}; identity {product}; {len(raw)} bytes")
    print(f"Build: #{build_number}; commit {build_sha[:8]}; SHA-256 {package_sha256}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    main(Path(sys.argv[1]), Path(sys.argv[2]))
