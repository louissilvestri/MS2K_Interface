# Installers

**Just want to install it?** Download the ready-to-run installers from the
[**Releases**](https://github.com/louissilvestri/MS2K_Interface/releases) page — no build
required:

| Download | Installs | Notes |
|----------|----------|-------|
| `MS2K_Interface-x.y.z-Setup.exe` | Standalone editor | Per-user, no admin; adds Start Menu (+ optional Desktop) shortcut |
| `MS2K-Editor-VST3-x.y.z-Setup.exe` | VST3 plugin | Installs into the system VST3 folder; elevates for admin. Re-scan plug-ins in your DAW afterward |

Both installers are **self-contained** — they bundle the statically-linked binaries, so there's
nothing else to install. Each registers in *Add/Remove Programs* with its own uninstaller.

---

## Building the installers (maintainers)
The `.iss` files here are [Inno Setup](https://jrsoftware.org/isinfo.php) scripts (the
installer *source*). To produce the `Setup.exe` files:

1. Build the app + plugin first (see the root README):
   ```sh
   cmake --build build --target MS2K_Interface
   cmake --build build --target MS2K_Plugin_VST3
   ```
2. Compile with Inno Setup's command-line compiler (`ISCC.exe`):
   ```sh
   ISCC installers/app.iss      # -> dist/MS2K_Interface-1.0.0-Setup.exe
   ISCC installers/vst3.iss     # -> dist/MS2K-Editor-VST3-1.0.0-Setup.exe
   ```
   (`dist/` is git-ignored; upload the resulting `.exe` to a GitHub Release.)

- `app.iss` — per-user install (`PrivilegesRequired=lowest`) to `{autopf}\MS2K Interface`,
  Start Menu + optional Desktop shortcuts.
- `vst3.iss` — installs the `MS2K Editor.vst3` bundle to `{commoncf}\VST3` (admin).
