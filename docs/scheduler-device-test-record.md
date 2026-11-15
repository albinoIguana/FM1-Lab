# Device scheduler test record

Record one row per controlled test after flashing a build from the same commit.

| Test | Active voices / engines | FX | CPU0 busy | CPU0 voice | CPU0 postmix | CPU2 job | CPU2 wait | Peak render | Deadline misses |
|---|---|---|---:|---:|---:|---:|---:|---:|---:|
| Idle | none | bypass | | | | | | | |
| FM6 polyphony | FM6 only | bypass | | | | | | | |
| Mixed engines | all four slots | bypass | | | | | | | |
| Chorus | mixed | chorus | | | | | | | |
| Delay | mixed | delay | | | | | | | |
| Reverb | mixed | reverb | | | | | | | |
| All effects | mixed | all enabled | | | | | | | |

Capture readings after the scheduler has warmed up for several callbacks. Keep
the number of held notes, velocities, patch levels, and effect settings the
same between comparisons. Idle CPU2=0 is expected and is not a failure.
