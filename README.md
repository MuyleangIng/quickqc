# QuickQC

QuickQC is a fast, offline clipboard manager for text and images, built with C++ and Qt6.

## Suggested GitHub Repository

- Name: `quickqc`
- Description: `Fast offline clipboard manager for text and images with Qt6 (C++)`

## What Is Included

- Native Qt Widgets app
- SQLite local storage (`quickqc.sqlite`)
- Realtime clipboard capture for text and images
- Image file copy capture from Finder/Explorer (`Cmd/Ctrl + C`)
- Per-item actions (`Copy`, `Edit`, `Pin`, `Delete`, `Preview`)
- Clean toast feedback for copy actions
- Theme chips (`System`, `Moon`, `Sun`)
- Optional GPU image preview (with runtime support check)
- Tray menu, startup option, update action dialog

## Supported Platforms

QuickQC is designed for:

- macOS (AMD64 + ARM64)
- Linux (AMD64 + ARM64)
- Windows (AMD64 + ARM64)

CI workflows are included for cross-platform builds and tag-based releases.

## Install (Easy)

### macOS via Homebrew tap

```bash
brew tap muyleanging/quickqc https://github.com/MuyleangIng/quickqc.git
brew install --cask quickqc
```

### Linux (Ubuntu/Debian and other distros)

```bash
curl -fsSL https://raw.githubusercontent.com/MuyleangIng/quickqc/main/scripts/install-linux.sh | bash
```

This script auto-detects `amd64`/`arm64`, installs Qt runtime libs on apt-based systems, and installs `quickqc` to `~/.local/bin`.

### Windows (PowerShell)

```powershell
irm https://raw.githubusercontent.com/MuyleangIng/quickqc/main/scripts/install-windows.ps1 | iex
```

This installs `quickqc.exe` to `%LOCALAPPDATA%\QuickQC\quickqc.exe`.

## First Open / Security Prompts

If the app is unsigned, OS security may block first launch.

### macOS

```bash
sudo xattr -dr com.apple.quarantine /Applications/quickqc.app
```

If you run the app from Downloads instead of Applications, use that path instead.

### Windows

If SmartScreen blocks launch:

```powershell
Unblock-File "$env:LOCALAPPDATA\QuickQC\quickqc.exe"
```

### Linux

If needed, make binary executable:

```bash
chmod +x ~/.local/bin/quickqc
```

## Update Commands

- macOS (Homebrew): `brew update && brew upgrade --cask quickqc`
- Linux: rerun `install-linux.sh` command above to pull the latest release
- Windows: rerun `install-windows.ps1` command above to pull the latest release

## Build

```bash
cmake -S . -B build
cmake --build build --config Release
```

Run:

```bash
./build/quickqc
```

On macOS bundle builds:

```bash
./build/quickqc.app/Contents/MacOS/quickqc
```

## Data Location

QuickQC stores data in your OS app-data directory with:

- app name: `quickqc`
- DB file: `quickqc.sqlite`

## Release Tag

Recommended first release tag:

```bash
git tag -a 0.1.0 -m "QuickQC 0.1.0"
git push origin 0.1.0
```

Pushing tag `0.1.0` triggers GitHub Actions to build and attach release artifacts for:

- macOS (AMD64 + ARM64)
- Windows (AMD64 + ARM64)
- Linux Ubuntu (AMD64 + ARM64)
- Linux Fedora (AMD64 + ARM64)

## Issues and Sponsorship

- Issues: use the templates in `.github/ISSUE_TEMPLATE/`
- PR template: `.github/pull_request_template.md`
- Sponsorship links: `.github/FUNDING.yml`

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for full version history.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## License

Apache License 2.0 — see [LICENSE](LICENSE) for details.

---

<div align="center">

Made with ❤️ by **[Ing Muyleang](https://muyleanging.com)** · **[KhmerStack](https://khmerstack.muyleanging.com)**

</div>
