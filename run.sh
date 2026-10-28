#!/bin/zsh
set -euo pipefail

project_dir="${0:A:h}"
"$project_dir/build.sh"
open "$project_dir/build/TennisGame.app"
