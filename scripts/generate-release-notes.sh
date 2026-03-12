#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 3 ]]; then
  echo "Usage: $0 <tag> [changelog_file] [output_file]" >&2
  exit 1
fi

TAG_NAME="$1"
CHANGELOG_FILE="${2:-CHANGELOG.md}"
OUTPUT_FILE="${3:-.github/release-notes.generated.md}"
NORMALIZED_TAG="${TAG_NAME#v}"

if [[ ! -f "$CHANGELOG_FILE" ]]; then
  echo "Changelog file not found: $CHANGELOG_FILE" >&2
  exit 1
fi

TMP_BODY="$(mktemp)"
trap 'rm -f "$TMP_BODY"' EXIT

set +e
awk -v ver="$NORMALIZED_TAG" '
BEGIN { in_section = 0; found = 0 }
$0 ~ "^## \\[" ver "\\]" {
  in_section = 1
  found = 1
  next
}
in_section && $0 ~ "^## \\[" {
  in_section = 0
}
in_section {
  print
}
END {
  if (!found) {
    exit 42
  }
}
' "$CHANGELOG_FILE" > "$TMP_BODY"
AWK_STATUS=$?
set -e

if [[ $AWK_STATUS -eq 42 ]]; then
  echo "Missing changelog section for version '$NORMALIZED_TAG' in $CHANGELOG_FILE" >&2
  exit 1
fi

if [[ $AWK_STATUS -ne 0 ]]; then
  echo "Failed to extract release notes from $CHANGELOG_FILE" >&2
  exit 1
fi

if ! grep -q '[^[:space:]]' "$TMP_BODY"; then
  echo "Changelog section for '$NORMALIZED_TAG' is empty." >&2
  exit 1
fi

{
  echo "## QuickQC $NORMALIZED_TAG"
  echo
  cat "$TMP_BODY"
} > "$OUTPUT_FILE"

echo "Generated release notes: $OUTPUT_FILE"
