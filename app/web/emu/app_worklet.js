// FM1-Lab browser-only timing hooks. The host simulator runs the CPU2 job
// synchronously; timing the two sections separately lets the page estimate
// the dual-core critical path as max(core 0, core 1), rather than their sum.
let renderStart = 0, renderMs = 0, cpu2Start = 0, cpu2Ms = 0;
const now = () => performance.now();

globalThis.fm1App = {
  env: {
    fm1lab_render_start: () => { renderStart = now(); },
    fm1lab_render_end: () => { renderMs += now() - renderStart; },
    fm1lab_cpu2_start: () => { cpu2Start = now(); },
    fm1lab_cpu2_end: () => { cpu2Ms += now() - cpu2Start; },
  },
  process() {
    renderMs = 0;
    cpu2Ms = 0;
  },
  metrics() {
    return { renderMs, cpu2Ms };
  },
};
