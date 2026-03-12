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
- Windows (AMD64)

CI workflows are included for cross-platform builds and tag-based releases.

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
git tag -a v0.1.0 -m "QuickQC v0.1.0"
git push origin v0.1.0
```

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
