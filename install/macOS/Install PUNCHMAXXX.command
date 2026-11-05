#!/bin/bash
# PUNCHMAXXX installer — TRIPLE SEVEN MAFIA
cd "$(dirname "$0")" || exit 1
echo ""
echo "  PUNCHMAXXX by TRIPLE SEVEN MAFIA — installing..."
echo ""

VST3="$HOME/Library/Audio/Plug-Ins/VST3"
AU="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3" "$AU"

ok=1
for pair in "PUNCHMAXXX.vst3|$VST3" "PUNCHMAXXX.component|$AU"; do
  src="${pair%%|*}"; dst="${pair##*|}"
  if [ -d "$src" ]; then
    rm -rf "$dst/$src"
    cp -R "$src" "$dst/" && xattr -dr com.apple.quarantine "$dst/$src" 2>/dev/null
    echo "  ✓ $src  ->  $dst"
  else
    echo "  ✗ $src not found next to this script"; ok=0
  fi
done

if [ -d "PUNCHMAXXX.app" ]; then
  rm -rf "/Applications/PUNCHMAXXX.app" 2>/dev/null
  if cp -R "PUNCHMAXXX.app" /Applications/ 2>/dev/null; then
    xattr -dr com.apple.quarantine /Applications/PUNCHMAXXX.app 2>/dev/null
    echo "  ✓ PUNCHMAXXX.app  ->  /Applications"
  else
    echo "  (skipped the standalone app — no permission for /Applications)"
  fi
fi

# make Logic / GarageBand pick up the new Audio Unit
killall -9 AudioComponentRegistrar >/dev/null 2>&1

echo ""
if [ $ok = 1 ]; then echo "  DONE. Restart your DAW and go make some drums slap."; else echo "  Finished with errors — see above."; fi
echo ""
