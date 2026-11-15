# Adaptive scheduler validation

The adaptive scheduler intentionally replaces the old deterministic 40/60
voice-count expectation. Host tests must validate invariants, not reproduce a
fixed split:

- with more than one active voice and a functioning worker, each quantum
  dispatches one CPU2 job;
- the CPU2 rollback snapshot never exceeds half the voice pool;
- CPU2 receives at least one voice when dispatch is active;
- fallback mode dispatches no CPU2 job and renders all voices on CPU0;
- output and voice lifecycle tests remain unchanged.

On-device validation should record the same workload before and after the
scheduler has learned its engine costs. Compare CPU0 active/voice/postmix,
CPU2 job/wait, dispatches, fallback count, render peak, and deadline misses.
Test each fixed engine slot alone and in mixed combinations, with effects
bypassed and enabled. Do not treat idle CPU2 time as unused render capacity:
it is expected when there are no active voices.
