#!/bin/zsh
set -euo pipefail

project_dir="${0:A:h}"
output_app="$project_dir/build/TennisGame.app"
stage_root="$(mktemp -d "${TMPDIR:-/tmp}/tennis-game-build.XXXXXX")"
trap 'rm -rf "$stage_root"' EXIT
app_dir="$stage_root/TennisGame.app"
contents_dir="$app_dir/Contents"

mkdir -p "$contents_dir/MacOS" "$contents_dir/Frameworks" "$contents_dir/Resources"

clang++ \
    -std=c++20 \
    -O2 \
    -Wall \
    -Wextra \
    -I"$project_dir/vendor/SDL3.framework/Headers" \
    -F"$project_dir/vendor" \
    "$project_dir/src/main.cpp" \
    "$project_dir/src/audio_system.cpp" \
    "$project_dir/src/ball_physics.cpp" \
    "$project_dir/src/game.cpp" \
    "$project_dir/src/gameplay_config.cpp" \
    "$project_dir/src/game_input.cpp" \
    "$project_dir/src/game_menu.cpp" \
    "$project_dir/src/game_render.cpp" \
    "$project_dir/src/progression_system.cpp" \
    "$project_dir/src/render_primitives.cpp" \
    "$project_dir/src/scoring_system.cpp" \
    -framework SDL3 \
    -Wl,-rpath,@executable_path/../Frameworks \
    -o "$contents_dir/MacOS/TennisGame"

cp "$project_dir/packaging/Info.plist" "$contents_dir/Info.plist"
mkdir -p "$contents_dir/Resources/audio"
cp "$project_dir/assets/audio/"*.wav "$contents_dir/Resources/audio/"
cp "$project_dir/assets/audio/LICENSE.txt" "$contents_dir/Resources/audio/"
ditto "$project_dir/vendor/SDL3.framework" "$contents_dir/Frameworks/SDL3.framework"
xattr -cr "$app_dir"
# iCloud Drive can immediately restore empty FinderInfo attributes after the
# generic cleanup; codesign treats those as forbidden resource metadata.
xattr -dr com.apple.FinderInfo "$app_dir" 2>/dev/null || true
codesign --force --deep --sign - "$app_dir" >/dev/null

if [[ -d "$output_app" && "$output_app" == "$project_dir/build/TennisGame.app" ]]; then
    rm -rf "$output_app"
fi
ditto "$app_dir" "$output_app"
# iCloud may attach provenance/Finder metadata again while copying the signed
# bundle into the workspace. Clean and sign the final artifact as well.
xattr -cr "$output_app"
xattr -dr com.apple.FinderInfo "$output_app" 2>/dev/null || true
codesign --force --deep --sign - "$output_app" >/dev/null

echo "Built: $output_app"
echo "Run with: open '$output_app'"
