# Changelog

All notable changes to QuickQC will be documented in this file.

The format is based on Keep a Changelog and this project uses Semantic Versioning.

## [0.2.1] - 2026-03-12

### Fixed

- Fixed in-app update download flow incorrectly reporting `Update download canceled.` when closing the progress dialog after a successful transfer.
- Improved cancellation handling so only true user-triggered cancel actions are treated as canceled downloads.

## [0.2.0] - 2026-03-12

### Added

- Added open-QuickQC hotkeys: `Cmd+Shift+V` on macOS and `Ctrl+Shift+V` on Windows.
- Added native global hotkey registration so QuickQC can be opened quickly while running.

### Changed

- Updated UI subtitle text to show the open shortcut.

## [0.1.10] - 2026-03-12

### Fixed

- Tray `Check Updates` now runs reliably by deferring action execution until after the tray menu closes.
- Tray `Open Settings` now opens more reliably with proper focus/order.
- Settings dialog positioning now clamps to the active screen to avoid partial/off-screen rendering.
- Update-check dialog now opens centered and in front consistently when triggered from tray.

## [0.1.9] - 2026-03-12

### Fixed

- Update-check dialog now closes reliably from both the `Close` button and window close (`X`) button.
- Stopped the updater UI from appearing as a duplicate embedded sheet; update check now uses a normal standalone modal flow.
- Settings window now opens in front more consistently and behaves as a normal closable modal dialog.

### Added

- macOS in-app updater flow now downloads and stages the correct release artifact directly, without opening the website.
- After download, updater now offers `Restart Now` (apply immediately) or `Later` (apply automatically when QuickQC exits).

## [0.1.8] - 2026-03-12

### Changed

- Reworked update checking into one modal flow: loading state first, then latest/update/error result in the same dialog.
- Update-available modal now provides `Update Now`, `Later`, and `Restart Now` actions.
- Up-to-date modal now clearly shows both current and latest version numbers.

## [0.1.7] - 2026-03-12

### Added

- Added live update checking against the latest GitHub release with loading state and clear up-to-date/update-available messages.

## [0.1.6] - 2026-03-12

### Fixed

- Added single-instance handling so launching QuickQC again focuses the running app instead of opening duplicates.
- Fixed "Start app when I log in" to stop immediately spawning another instance when toggled on.
- Improved "Open Settings" behavior from tray by showing/focusing the main window first.

## [0.1.5] - 2026-03-12

### Fixed

- Removed unsupported `EXCLUDE_PLUGINS` argument for Qt 6.7.3 deployment script generation.
- Keeps the improved macOS bundle verification and packaging flow from prior fixes.

## [0.1.4] - 2026-03-12

### Fixed

- macOS release verification now accepts QtSql links via `@rpath` or `@executable_path`.
- CI now verifies bundled `QtSql.framework` and `qsqlite` driver files before packaging.
- macOS deploy now excludes `qsqlmimer` to avoid unresolved external driver dependency noise.

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
