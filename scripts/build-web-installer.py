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

    # Read-only hardware diagnostics share the installer's existing Web MIDI page.
    # The firmware answers fm1-base's established LG SysEx log request (F0 7D 4C 47 F7).
    diagnostics_panel = r"""
  <section id="fm1-diagnostics" aria-label="FM1 hardware diagnostics" style="margin: 1.25rem 0; padding: 1rem; border: 1px solid #59616b; border-radius: 12px; background: rgba(127,127,127,.08)">
    <h2 style="margin: 0 0 .4rem">Hardware diagnostics</h2>
    <p style="margin: 0 0 .8rem; font-size: .9rem">Connect your FM1 by USB, grant Web MIDI access, then record live audio-engine measurements directly from the device. This is read-only: it does not send notes or change sounds.</p>
    <div style="display: grid; grid-template-columns: repeat(auto-fit,minmax(190px,1fr)); gap: .65rem; align-items: end">
      <label style="display:grid; gap:.25rem; font-size:.88rem">FM1 MIDI port
        <select id="fm1diag-port" style="padding:.55rem; min-width:0; width:100%"><option value="">Connect to list ports…</option></select>
      </label>
      <label style="display:grid; gap:.25rem; font-size:.88rem">Test stage
        <select id="fm1diag-stage" style="padding:.55rem; min-width:0; width:100%">
          <option>Idle / FX bypassed</option><option>Held notes / FX bypassed</option><option>Increasing polyphony</option><option>Both sequencer tracks</option><option>Chorus</option><option>Delay</option><option>Reverb</option><option>Phaser</option><option>Amp, EQ and compressor</option><option>Full-load stress test</option>
        </select>
      </label>
      <label style="display:grid; gap:.25rem; font-size:.88rem">Stage notes
        <input id="fm1diag-notes" type="text" maxlength="120" placeholder="Instrument, chord, FX settings…" style="padding:.55rem; min-width:0; width:100%; box-sizing:border-box">
      </label>
    </div>
    <div style="display:flex; flex-wrap:wrap; gap:.5rem; margin:.8rem 0">
      <button id="fm1diag-connect" type="button">Connect FM1</button>
      <button id="fm1diag-start" type="button" disabled>Start recording</button>
      <button id="fm1diag-stop" type="button" disabled>Stop</button>
      <button id="fm1diag-download" type="button" disabled>Download diagnostics CSV</button>
      <button id="fm1diag-events-download" type="button" disabled>Download key events CSV</button>
    </div>
    <p id="fm1diag-status" role="status" aria-live="polite" style="font-size:.9rem; margin:.35rem 0">Not connected. Use a desktop browser with Web MIDI support (Chrome or Edge recommended) over HTTPS.</p>
    <div id="fm1diag-live" style="display:grid; grid-template-columns:repeat(auto-fit,minmax(135px,1fr)); gap:.5rem; margin:.75rem 0"></div>
    <p id="fm1diag-count" style="font-size:.82rem; margin:.35rem 0">0 samples recorded · 0 key events</p>
    <details><summary>What is recorded?</summary><p style="font-size:.88rem">Render time and budget, deadline misses, CPU0 busy/voice/postmix time, CPU2 job/wait/backup/fallback metrics, voice peak and shedding, CPU clock/load, DMA xruns, pre-limiter peak and limiter reduction. Each row also includes host UTC time, device uptime, test stage and your notes. Cumulative counters are since device boot.</p></details>
  </section>
  <script>
  (() => {
    const $ = id => document.getElementById("fm1diag-" + id);
    const status = $("status"), portSelect = $("port"), startBtn = $("start"), stopBtn = $("stop"), downloadBtn = $("download");
    let access = null, input = null, output = null, listener = null, timer = null, recording = false, pending = null, rows = [], sampleCount = 0;
    let keyEvents = [], recordingStarted = 0;
    const setStatus = s => { status.textContent = s; };
    const say = (s, v) => { const e = document.createElement("div"); e.style.cssText = "padding:.55rem;border:1px solid #59616b;border-radius:8px;font-size:.82rem;overflow-wrap:anywhere"; e.textContent = s + ": " + v; $("live").appendChild(e); };
    const csvCell = v => '"' + String(v ?? "").replace(/"/g, '""') + '"';
    function stop() {
      recording = false; if (timer) clearTimeout(timer); timer = null; pending = null;
      startBtn.disabled = !input || !output; stopBtn.disabled = true;
      setStatus("Recording stopped. " + rows.length + " snapshots captured.");
    }
    function snapshot() {
      if (!recording || !output || pending) return;
      pending = { values: {}, started: performance.now() };
      try { output.send([0xF0, 0x7D, 0x4C, 0x47, 0xF7]); }
      catch (e) { pending = null; stop(); setStatus("USB-MIDI send failed: " + e.message); }
      if (pending) timer = setTimeout(() => {
        if (!pending) return;
        pending = null;
        setStatus("No diagnostic response. Check that this FM1 is running an FM1-Lab build with telemetry support.");
        timer = setTimeout(snapshot, 1000);
      }, 1800);
    }
    function onMessage(event) {
      const d = event.data;
      if (!d || !d.length) return;
      // The FM1 keybed already mirrors note-on/off and poly-aftertouch to USB MIDI.
      // Capture those incoming device events without adding firmware/audio-thread work.
      if (d[0] !== 0xF0) {
        if (!recording) return;
        const statusByte = d[0], kind = statusByte & 0xF0, channel = (statusByte & 0x0F) + 1;
        if ((kind === 0x80 || kind === 0x90 || kind === 0xA0) && d.length >= 3) {
          const value = d[2], isOff = kind === 0x80 || (kind === 0x90 && value === 0);
          keyEvents.push({
            host_time_utc: new Date().toISOString(),
            elapsed_ms: Math.round(performance.now() - recordingStarted),
            event: kind === 0xA0 ? "poly_aftertouch" : (isOff ? "note_off" : "note_on"),
            channel, note: d[1] & 0x7F, value,
            source_port: input ? input.name : ""
          });
          $("count").textContent = sampleCount + " samples recorded · " + keyEvents.length + " key events";
          $("events-download").disabled = false;
        }
        return;
      }
      if (d.length < 5 || d[1] !== 0x7D || d[2] !== 0x4C || d[3] !== 0x47) return;
      const payload = new TextDecoder().decode(d.slice(4, d[d.length - 1] === 0xF7 ? -1 : undefined));
      if (!pending) return;
      if (payload === "end") {
        clearTimeout(timer); timer = null;
        const values = pending.values;
        const elapsed = performance.now() - pending.started;
        pending = null;
        if (Object.keys(values).length) {
          const row = { host_time_utc: new Date().toISOString(), stage: $("stage").value, notes: $("notes").value, ...values };
          rows.push(row); sampleCount++;
          $("count").textContent = sampleCount + " samples recorded · " + keyEvents.length + " key events";
          $("live").replaceChildren();
          ["render_us","render_budget_us","cpu0_busy_us","cpu2_job_sum_us","voice_peak","xruns","deadline_misses","cpu2_fallbacks","shed_total"].forEach(k => { if (values[k] !== undefined) say(k.replaceAll("_"," "), values[k]); });
          downloadBtn.disabled = false;
          setStatus("Recording · " + sampleCount + " snapshots · " + row.host_time_utc);
        }
        if (recording) timer = setTimeout(snapshot, Math.max(0, 1000 - elapsed));
        return;
      }
      const split = payload.indexOf("=");
      if (split > 0) {
        const key = payload.slice(0, split), value = Number(payload.slice(split + 1));
        if (Number.isFinite(value)) pending.values[key] = value;
      }
    }
    function connect() {
      if (!navigator.requestMIDIAccess) { setStatus("Web MIDI is unavailable. Use Chrome or Edge on desktop over HTTPS."); return; }
      navigator.requestMIDIAccess({ sysex: true }).then(midi => {
        access = midi;
        const ins = [...midi.inputs.values()], outs = [...midi.outputs.values()];
        const pairs = ins.map(i => ({ input:i, output:outs.find(o => o.name === i.name) || (outs.length === 1 ? outs[0] : null) })).filter(p => p.output);
        portSelect.replaceChildren();
        pairs.forEach((p,i) => { const o=document.createElement("option"); o.value=String(i); o.textContent=p.input.name || "USB MIDI port"; portSelect.appendChild(o); });
        if (!pairs.length) { setStatus("No paired MIDI input/output ports found. Connect the FM1 by USB and try again."); return; }
        portSelect._pairs = pairs;
        setStatus(pairs.length + " MIDI port(s) found. Select the FM1 and connect.");
        $("connect").textContent = "Connect selected port";
      }).catch(e => setStatus("MIDI access was not granted: " + e.message));
    }
    $("connect").addEventListener("click", () => {
      if (!access) { connect(); return; }
      const pair = portSelect._pairs && portSelect._pairs[Number(portSelect.value)];
      if (!pair) { setStatus("Select an FM1 MIDI port first."); return; }
      if (input && listener) input.removeEventListener("midimessage", listener);
      input = pair.input; output = pair.output; listener = onMessage;
      Promise.all([input.open(), output.open()]).then(() => {
        input.addEventListener("midimessage", listener);
        startBtn.disabled = false; $("connect").disabled = true; portSelect.disabled = true;
        setStatus("Connected to " + (input.name || "FM1") + ". Ready to record.");
      }).catch(e => setStatus("Could not open MIDI port: " + e.message));
    });
    startBtn.addEventListener("click", () => {
      if (!input || !output) return;
      recording = true; recordingStarted = performance.now(); startBtn.disabled = true; stopBtn.disabled = false;
      setStatus("Recording diagnostics and keybed MIDI events…"); snapshot();
    });
    stopBtn.addEventListener("click", stop);
    function downloadCsv(data, filename) {
      if (!data.length) return;
      const keys = [...new Set(data.flatMap(r => Object.keys(r)))];
      const csv = [keys.map(csvCell).join(","), ...data.map(r => keys.map(k => csvCell(r[k])).join(","))].join("\r\n");
      const blob = new Blob([csv], {type:"text/csv;charset=utf-8"});
      const url = URL.createObjectURL(blob), a = document.createElement("a");
      a.href = url; a.download = filename + "-" + new Date().toISOString().replace(/[:.]/g,"-") + ".csv";
      a.click(); setTimeout(() => URL.revokeObjectURL(url), 1000);
    }
    downloadBtn.addEventListener("click", () => downloadCsv(rows, "fm1-diagnostics"));
    $("events-download").addEventListener("click", () => downloadCsv(keyEvents, "fm1-key-events"));
    if (!window.isSecureContext) setStatus("Web MIDI requires a secure HTTPS page.");
  })();
  </script>
"""
    if html_doc.count("</body>") != 1:
        raise SystemExit("Expected exactly one closing body tag in upstream installer")
    html_doc = html_doc.replace("</body>", diagnostics_panel + "\n</body>")

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
