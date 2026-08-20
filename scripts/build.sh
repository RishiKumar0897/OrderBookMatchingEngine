#!/bin/bash

#exit immediately if a command exits with a non-zero status
set -e

#change to the root directory of the project
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

#defaults
BUILD_DIR="$PROJECT_ROOT/build"
TARGET=""
CONFIG="Release"

#help msg
print_help() {
  echo "Usage: $0 [options]"
  echo "Options:"
  echo "  -t, --target <target>   Specify the build target (e.g., 'all', 'clean', 'install')"
  echo "  -c, --config <config>   Specify the build configuration (e.g., 'Debug', 'Release')"
  echo "  -h, --help              Show this help message"
}

#parse command line arguments
while getopts "t:c:h" opt; do
  case $opt in
    t)
      TARGET="$OPTARG"
      ;;
    c)
      CONFIG="$OPTARG"
      ;;
    h)
      print_help
      exit 0
      ;;
    *)
      print_help
      exit 1
      ;;
  esac
done

#standardize capitalization of the config
CONFIG=$(echo "$CONFIG" | tr '[:lower:]' '[:upper:]')

#isolate build directories by configuration to prevent caching bugs
BUILD_DIR="$BUILD_DIR/$CONFIG"

#detect cpu cores for parallel builds
NUM_CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)

#configure the project
echo "--> Configuring prroject in: $BUILD_DIR"
echo "--> Build Mode: $CONFIG"
cmake -S "$PROJECT_ROOT/matching-engine" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CONFIG"

#build the project
if [ -n "$TARGET" ]; then
    echo "--> Building target: $TARGET (using $NUM_CORES cores)"
    cmake --build "$BUILD_DIR" --target "$TARGET" -- -j"$NUM_CORES"
else
    echo "--> Building all targets (using $NUM_CORES cores)"
    cmake --build "$BUILD_DIR" -- -j"$NUM_CORES"
fi

echo "--> Build completed successfully. Files are located in: $BUILD_DIR"

