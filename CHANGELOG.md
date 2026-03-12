# Changelog

All notable changes to QuickQC will be documented in this file.

The format is based on Keep a Changelog and this project uses Semantic Versioning.

## [0.1.3] - 2026-03-12

### Fixed

- macOS release packaging now installs/deploys the app bundle before archiving it.
- CI now validates the macOS binary links `QtSql.framework` from inside the app bundle.
- Homebrew macOS installs now receive self-contained `.app` artifacts instead of CI-path linked binaries.

## [0.1.0] - 2026-03-12

### Added

- Initial public QuickQC Qt release
- Clipboard history for text and images
- Per-item actions (copy, edit, pin, delete, preview)
- Theme chips (System / Moon / Sun)
- Settings for startup, auto-close, and GPU preview
- Tray integration and copy toast feedback
- Cross-platform CI and tag-based release workflow templates
