#!/bin/zsh
set -euo pipefail

project_dir="${0:A:h}"
test_dir="$project_dir/build/tests"
mkdir -p "$test_dir"

clang++ \
    -std=c++20 \
    -O2 \
    -Wall \
    -Wextra \
    -I"$project_dir/src" \
    "$project_dir/tests/core_tests.cpp" \
    "$project_dir/src/ball_physics.cpp" \
    "$project_dir/src/gameplay_config.cpp" \
    "$project_dir/src/progression_system.cpp" \
    "$project_dir/src/scoring_system.cpp" \
    -o "$test_dir/core_tests"

"$test_dir/core_tests"
echo "Core tests passed"
