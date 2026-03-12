## QuickQC 0.1.0

QuickQC is a native C++/Qt clipboard manager focused on speed, local-first privacy, and compact workflow.

### Highlights

- Realtime text + image clipboard history
- Per-item actions: copy, edit, pin, delete, preview
- Image source path support and image preview dialog
- Theme chips: `System`, `Moon`, `Sun`
- Copy feedback with centered toast and status timestamp
- Tray menu controls, startup option, auto-close behavior
- Optional GPU image preview with runtime support detection

### Build Artifacts

This release pipeline publishes binaries for:

- macOS AMD64 (`macos-13`)
- macOS ARM64 (`macos-14`)
- Windows AMD64 (`windows-latest`)
- Linux Ubuntu AMD64 (`ubuntu-latest`)
- Linux Ubuntu ARM64 (`ubuntu-24.04-arm`)
- Linux Fedora AMD64 (`fedora:41`)
- Linux Fedora ARM64 (`fedora:41` on ARM runner)

### Notes

- Linux artifacts are generic binaries packaged as `.tar.gz`.
- Windows artifact is packaged as `.zip`.
- macOS artifact is packaged as `.app` bundle archive.

Made with ❤️ by **Ing Muyleang** · **KhmerStack**
