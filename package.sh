#!/bin/bash
set -e

####################################
# Script đóng gói SDK (Alias cho build_sdk.sh)
# Sử dụng build_sdk.sh để có nhiều tùy chọn hơn
####################################

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Default install prefix
INSTALL_PREFIX=$(pwd)/output

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --prefix)
            INSTALL_PREFIX="$2"
            shift 2
            ;;
        *)
            # Forward all other arguments to build_sdk.sh
            break
            ;;
    esac
done

echo "=========================================="
echo "  CVEDIX SDK Packaging Script"
echo "=========================================="
echo ""
echo "Note: This script is a wrapper for build_sdk.sh"
echo "For more options, use: ./build_sdk.sh --help"
echo ""

# Call build_sdk.sh with arguments
"$SCRIPT_DIR/build_sdk.sh" --prefix="$INSTALL_PREFIX" --build-type=Release "$@"
