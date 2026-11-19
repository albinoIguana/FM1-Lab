# Subtractive patches

Subtractive patches are stored as engine-native parameter records in this catalog. Organize family subfolders (bass, leads, pads, keys, sequences, etc.) using the same README + `patches.json` convention as the DX7 archive. Do not store DX7 voice payloads here.


The active starter catalog contains four native Subtractive patches. Each family folder stores a `patches.json` file with a stable engine-prefixed ID and the ten synthesis parameters consumed by the Subtractive slot. The bundled FM1 preset browser currently exposes these four curated entries.


The starter bank is intentionally small: four authored Subtractive patches (ANALOG BS, SAW LEAD, WARM PAD and SYNC PLUK). The engine slot selects this catalog; the preset browser applies the parameter payload, not just the label.
