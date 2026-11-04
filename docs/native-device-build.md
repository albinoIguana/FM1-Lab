# Native FM1 device build

The browser/WASI simulator build is not a device firmware build. The native entrypoint stages the FM1-Lab C modules into the pinned M-VAVE application tree, applies the storage and physical-control source patches, then invokes the matching SDK firmware-only build.

From Windows PowerShell at the repository root:

```powershell
git submodule update --init --recursive
.\scripts\build-device-firmware.ps1 -Toolchain 'C:\path\to\JieLi\pi32' 
```

The toolchain must be the exact JieLi Windows PI32v2 toolchain 2.5.3 required by the pinned SDK manifest. The script deliberately uses `-FirmwareOnly`: it does not create a distribution package or alter the live/main branch.

The script currently establishes the native compilation path for the Instrument data model, metadata codec, and named physical-control constants. It does **not** mean the new four-patch Instrument is already connected to note allocation, track mixing, or the device HOME screen. Those integrations and actual-hardware flash/recovery checks remain separate work.

The native SDK build may not run in GitHub's Linux browser-simulator workflow: it requires Windows and the exact external JieLi toolchain. A successful simulator build must never be reported as a successful device firmware build.
