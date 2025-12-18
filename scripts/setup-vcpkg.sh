#!/usr/bin/env bash
set -euo pipefail

# RealCraft vcpkg Setup Script
# Clones and bootstraps vcpkg in the project directory

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
VCPKG_DIR="${PROJECT_ROOT}/vcpkg"

echo "=========================================="
echo "RealCraft vcpkg Setup"
echo "=========================================="

if [[ -d "$VCPKG_DIR" ]]; then
    echo "vcpkg already exists at $VCPKG_DIR"
    echo "Updating..."
    cd "$VCPKG_DIR"
    git pull
else
    echo "Cloning vcpkg..."
    git clone https://github.com/microsoft/vcpkg.git "$VCPKG_DIR"
    cd "$VCPKG_DIR"
fi

echo ""
echo "Bootstrapping vcpkg..."
if [[ "$(uname)" == "Darwin" ]] || [[ "$(uname)" == "Linux" ]]; then
    ./bootstrap-vcpkg.sh -disableMetrics
else
    ./bootstrap-vcpkg.bat -disableMetrics
fi

echo ""
echo "=========================================="
echo "vcpkg setup complete!"
echo ""
echo "Add the following to your shell profile:"
echo "  export VCPKG_ROOT=\"$VCPKG_DIR\""
echo ""
echo "Or run builds with the local vcpkg:"
echo "  ./scripts/build.sh"
echo "=========================================="
