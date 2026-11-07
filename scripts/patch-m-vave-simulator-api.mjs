import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const vendor = path.join(root, "vendor/m-vave-fm1");

function patchFile(relativePath, transform) {
  const filename = path.join(vendor, relativePath);
  const original = fs.readFileSync(filename, "utf8");
  const updated = transform(original);
  if (updated !== original) fs.writeFileSync(filename, updated);
}

patchFile("web/engine/Simulator.c", source => {
  if (source.includes('EXPORT("sim_fm_factory")')) return source;
  const anchor = "int fm1_usb_audio_read(int32_t *pcm, unsigned frames, uint16_t gains[2])";
  if (source.indexOf(anchor) < 0 || source.indexOf(anchor, source.indexOf(anchor) + anchor.length) >= 0) {
    throw new Error("Simulator API patch: expected one USB audio adapter anchor.");
  }
  const addition = `/* FM1-Lab reads factory voices through the native 156-byte DX7 format. */
EXPORT("sim_fm_factory") int sim_fm_factory(unsigned program)
{
    groove_fm_factory(file_buffer, program % FM1_FM_PROGRAMS);
    return 0;
}

`;
  return source.replace(anchor, addition + anchor);
});

patchFile("web/synth/dx7core.c", source => {
  if (source.includes("unsigned sim_fm_operator_depth(")) return source;
  const anchor = "};\n\nFM1_DSP_CODE static void compute_fb4";
  if (source.indexOf(anchor) < 0 || source.indexOf(anchor, source.indexOf(anchor) + anchor.length) >= 0) {
    throw new Error("DX7 topology patch: expected algorithm routing table terminator.");
  }
  const addition = `};

/* Export topology metadata from the same routing table used by the FM engine.
 * Carrier bits use printed OP1..OP6 numbering (bit zero is OP1). */
#ifdef __wasm__
__attribute__((export_name("sim_fm_carriers")))
#endif
unsigned sim_fm_carriers(unsigned algorithm)
{
    const u8 *flags = algorithms[algorithm & 31U];
    unsigned mask = 0;
    for (unsigned op = 0; op < FM_OP_CNT; ++op)
        if ((flags[op] & 3U) == 0) mask |= 1U << (5U - op);
    return mask;
}

#ifdef __wasm__
__attribute__((export_name("sim_fm_operator_depth")))
#endif
unsigned sim_fm_operator_depth(unsigned algorithm, unsigned operator_number)
{
    if (operator_number < 1U || operator_number > FM_OP_CNT) return 0U;
    const u8 *flags = algorithms[algorithm & 31U];
    u8 producers[3][FM_OP_CNT] = {{0}};
    u8 counts[3] = {0};
    u8 edges[FM_OP_CNT][FM_OP_CNT] = {{0}};
    u8 carrier[FM_OP_CNT] = {0};
    u8 depth[FM_OP_CNT] = {0};
    for (unsigned op = 0; op < FM_OP_CNT; ++op) {
        unsigned inbus = (flags[op] >> 4) & 3U;
        unsigned outbus = flags[op] & 3U;
        unsigned add = (flags[op] & 4U) != 0U;
        if (inbus)
            for (unsigned n = 0; n < counts[inbus]; ++n)
                edges[producers[inbus][n]][op] = 1U;
        if (!outbus) carrier[op] = 1U;
        else {
            if (!add) counts[outbus] = 0U;
            producers[outbus][counts[outbus]++] = (u8)op;
        }
    }
    for (unsigned op = 0; op < FM_OP_CNT; ++op)
        depth[op] = carrier[op] ? 0U : 1U;
    for (unsigned pass = 0; pass < FM_OP_CNT; ++pass)
        for (unsigned op = 0; op < FM_OP_CNT; ++op)
            for (unsigned next = 0; next < FM_OP_CNT; ++next)
                if (edges[op][next] && depth[next] + 1U > depth[op])
                    depth[op] = (u8)(depth[next] + 1U);
    return depth[FM_OP_CNT - operator_number];
}

FM1_DSP_CODE static void compute_fb4`;
  return source.replace(anchor, addition);
});

patchFile("sdk/Simulator.js", source => {
  if (source.includes("typeof document === 'undefined' || globalThis.FM1_LAB_MODE")) return source;
  const before = "if (typeof document === 'undefined') return;";
  if (source.indexOf(before) < 0 || source.indexOf(before, source.indexOf(before) + before.length) >= 0) {
    throw new Error("Simulator JS patch: expected one browser UI bootstrap guard.");
  }
  return source.replace(before, "if (typeof document === 'undefined' || globalThis.FM1_LAB_MODE) return;");
});

console.log("Applied idempotent FM1-Lab simulator API and topology integration.");
