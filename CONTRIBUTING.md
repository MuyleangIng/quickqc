# Contributing to QuickQC

Thanks for contributing.

## Before You Start

- Open an issue first for major changes.
- Keep pull requests focused and small.
- Write clear commit messages.

## Development Setup

```bash
cmake -S . -B build
cmake --build build --config Release
```

## Coding Guidelines

- Language: C++20 + Qt6
- Keep UI responsive and lightweight
- Prefer clear naming over clever code
- Add comments only where logic is non-obvious

## Pull Request Checklist

- Builds successfully on your machine
- No unrelated file changes
- Updated docs if behavior changed
- Added or updated tests when applicable

## Reporting Bugs

Please include:

- OS and version
- App version
- Steps to reproduce
- Expected result vs actual result
- Screenshot or logs (if possible)
