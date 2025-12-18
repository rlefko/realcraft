#!/usr/bin/env bash
set -euo pipefail

# RealCraft Build Script for macOS/Linux
# Usage: ./scripts/build.sh [debug|release] [--clean] [--test]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Defaults
BUILD_TYPE="debug"
CLEAN_BUILD=false
RUN_TESTS=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        debug|Debug)
            BUILD_TYPE="debug"
            shift
            ;;
        release|Release)
            BUILD_TYPE="release"
            shift
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --test)
            RUN_TESTS=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [debug|release] [--clean] [--test]"
            echo ""
            echo "Options:"
            echo "  debug|release  Build configuration (default: debug)"
            echo "  --clean        Remove build directory before building"
            echo "  --test         Run tests after building"
            echo ""
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use -h or --help for usage information"
            exit 1
            ;;
    esac
done

# Detect platform
if [[ "$(uname)" == "Darwin" ]]; then
    PLATFORM="macos"
elif [[ "$(uname)" == "Linux" ]]; then
    PLATFORM="linux"
else
    echo "Unsupported platform: $(uname)"
    exit 1
fi

PRESET="${PLATFORM}-${BUILD_TYPE}"
BUILD_DIR="${PROJECT_ROOT}/build/${PRESET}"

echo "=========================================="
echo "RealCraft Build"
echo "Platform:      ${PLATFORM}"
echo "Configuration: ${BUILD_TYPE}"
echo "Preset:        ${PRESET}"
echo "=========================================="

# Ensure vcpkg is available
if [[ -z "${VCPKG_ROOT:-}" ]]; then
    if [[ -d "${PROJECT_ROOT}/vcpkg" ]]; then
        export VCPKG_ROOT="${PROJECT_ROOT}/vcpkg"
        echo "Using local vcpkg: ${VCPKG_ROOT}"
    else
        echo "Error: VCPKG_ROOT not set and vcpkg not found in project root"
        echo "Run: ./scripts/setup-vcpkg.sh"
        exit 1
    fi
fi

# Clean if requested
if [[ "$CLEAN_BUILD" == true ]] && [[ -d "$BUILD_DIR" ]]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Configure
echo ""
echo "Configuring..."
cmake --preset "$PRESET"

# Build
echo ""
echo "Building..."
cmake --build --preset "$PRESET" --parallel

# Test if requested
if [[ "$RUN_TESTS" == true ]]; then
    echo ""
    echo "Running tests..."
    ctest --preset "$PRESET" || true
fi

echo ""
echo "=========================================="
echo "Build complete!"
echo "Output: ${BUILD_DIR}"
echo "=========================================="
