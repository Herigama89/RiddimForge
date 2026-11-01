#!/bin/bash
# Installs RiddimForge (AU + VST3) for the current user and clears the "downloaded from the internet" flag.
# Run it with: right-click > Open (first time), or in Terminal: bash Install-RiddimForge.command
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
AU="$HOME/Library/Audio/Plug-Ins/Components"
VST3="$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$AU" "$VST3"
rm -rf "$AU/RiddimForge.component" "$VST3/RiddimForge.vst3"
cp -R "$HERE/RiddimForge.component" "$AU/"
cp -R "$HERE/RiddimForge.vst3" "$VST3/"
xattr -dr com.apple.quarantine "$AU/RiddimForge.component" "$VST3/RiddimForge.vst3" 2>/dev/null || true
xattr -dr com.apple.quarantine "$HERE/RiddimForge.app" 2>/dev/null || true
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo ""
echo "RiddimForge installed:"
echo "  AU   -> $AU/RiddimForge.component   (Logic, LUNA, FL Studio)"
echo "  VST3 -> $VST3/RiddimForge.vst3        (Cubase, FL Studio, LUNA)"
echo ""
echo "Logic: Settings > Plug-in Manager > Reset & Rescan Selection if it doesn't show up."
