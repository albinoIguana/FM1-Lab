# PSG patches

PSG patches are stored as engine-native parameter records in this catalog. Organize family subfolders (bass, leads, percussion, arps, effects, etc.) using the same README + `patches.json` convention as the DX7 archive. Do not store DX7 voice payloads here.


The active starter catalog contains four native PSG patches. Each family folder stores a `patches.json` file with a stable engine-prefixed ID and the ten synthesis parameters consumed by the PSG slot. The bundled FM1 preset browser currently exposes these four curated entries.


The starter bank is intentionally small: four authored PSG patches (CHIP BASS, NOISE SNAR, ARCADE LEAD and 8BIT ARP). The engine slot selects this catalog; the preset browser applies the parameter payload, not just the label.
