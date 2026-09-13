#!/bin/bash

#command to run
#example usage: ./scripts/clean.sh -c Release
#example usage: ./scripts/clean.sh --all
#exit immediately if a command exits with a non-zero status
set -e

#change to the root directory of the project
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

#defaults
BUILD_DIR="$PROJECT_ROOT/build"
CONFIG=""
CLEAN_ALL=0

#help msg
print_help() {
  echo "Usage: $0 [options]"
  echo "Options:"
  echo "  -c, --config <config>   Remove only this configuration's build dir (e.g., 'Debug', 'Release')"
  echo "  -a, --all               Remove the entire build directory (all configurations)"
  echo "  -h, --help              Show this help message"
}

#parse command line arguments
while getopts "c:ah-:" opt; do
  case $opt in
    c)
      CONFIG="$OPTARG"
      ;;
    a)
      CLEAN_ALL=1
      ;;
    h)
      print_help
      exit 0
      ;;
    -)
      case "$OPTARG" in
        config)
          CONFIG="${!OPTIND}"; OPTIND=$((OPTIND + 1))
          ;;
        all)
          CLEAN_ALL=1
          ;;
        help)
          print_help
          exit 0
          ;;
        *)
          print_help
          exit 1
          ;;
      esac
      ;;
    *)
      print_help
      exit 1
      ;;
  esac
done

if [ "$CLEAN_ALL" -eq 1 ]; then
  if [ -d "$BUILD_DIR" ]; then
    echo "--> Removing entire build directory: $BUILD_DIR"
    rm -rf "$BUILD_DIR"
    echo "--> Clean completed successfully."
  else
    echo "--> Nothing to clean. Build directory does not exist: $BUILD_DIR"
  fi
  exit 0
fi

if [ -z "$CONFIG" ]; then
  print_help
  exit 1
fi

#standardize capitalization of the config, matching build.sh
CONFIG=$(echo "$CONFIG" | tr '[:lower:]' '[:upper:]')
TARGET_DIR="$BUILD_DIR/$CONFIG"

if [ -d "$TARGET_DIR" ]; then
  echo "--> Removing build directory: $TARGET_DIR"
  rm -rf "$TARGET_DIR"
  echo "--> Clean completed successfully."
else
  echo "--> Nothing to clean. Build directory does not exist: $TARGET_DIR"
fi
