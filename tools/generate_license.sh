#!/bin/bash

# CVEDIX License Generator Script
# Tạo license file cho CVEDIX AI Runtime SDK sử dụng licensecxx

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default values
PRIVATE_KEY="./private_key.pem"
PUBLIC_KEY="./public_key.pem"
OUTPUT_LICENSE="./license.lic"
EXPIRATION_DATE=""
FEATURES="tensorrt,rknn,face_recognition"

# Function to print usage
usage() {
    echo "CVEDIX License Generator"
    echo ""
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -k, --private-key PATH    Path to private key file (default: ./private_key.pem)"
    echo "  -o, --output PATH         Output license file path (default: ./license.lic)"
    echo "  -e, --expiration DATE     Expiration date in YYYY-MM-DD format (required)"
    echo "  -f, --features LIST       Comma-separated feature list (default: tensorrt,rknn,face_recognition)"
    echo "  -g, --generate-keys       Generate new RSA key pair"
    echo "  -h, --help                Show this help message"
    echo ""
    echo "Examples:"
    echo "  # Generate new keys and license"
    echo "  $0 --generate-keys --expiration 2025-12-31"
    echo ""
    echo "  # Generate license with existing keys"
    echo "  $0 --private-key ./keys/private_key.pem --expiration 2025-12-31"
    echo ""
    echo "  # Generate license with specific features"
    echo "  $0 --expiration 2025-12-31 --features tensorrt,rknn"
}

# Function to generate RSA key pair
generate_keys() {
    echo -e "${GREEN}Generating RSA key pair...${NC}"
    
    if [ -f "$PRIVATE_KEY" ]; then
        echo -e "${YELLOW}Warning: Private key file already exists: $PRIVATE_KEY${NC}"
        read -p "Overwrite? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            echo "Aborted."
            exit 1
        fi
    fi
    
    # Generate private key (1024-bit RSA)
    openssl genrsa -out "$PRIVATE_KEY" 1024
    
    # Generate public key from private key
    openssl rsa -in "$PRIVATE_KEY" -pubout -out "$PUBLIC_KEY"
    
    echo -e "${GREEN}✓ Keys generated successfully!${NC}"
    echo "  Private key: $PRIVATE_KEY"
    echo "  Public key:  $PUBLIC_KEY"
    echo ""
    echo -e "${YELLOW}⚠️  IMPORTANT: Keep private key secure! Never distribute it.${NC}"
    echo ""
}

# Parse command line arguments
GENERATE_KEYS=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -k|--private-key)
            PRIVATE_KEY="$2"
            shift 2
            ;;
        -o|--output)
            OUTPUT_LICENSE="$2"
            shift 2
            ;;
        -e|--expiration)
            EXPIRATION_DATE="$2"
            shift 2
            ;;
        -f|--features)
            FEATURES="$2"
            shift 2
            ;;
        -g|--generate-keys)
            GENERATE_KEYS=true
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            usage
            exit 1
            ;;
    esac
done

# Generate keys if requested
if [ "$GENERATE_KEYS" = true ]; then
    generate_keys
fi

# Check if expiration date is provided
if [ -z "$EXPIRATION_DATE" ]; then
    echo -e "${RED}Error: Expiration date is required!${NC}"
    echo "Use --expiration YYYY-MM-DD"
    usage
    exit 1
fi

# Validate expiration date format
if ! [[ "$EXPIRATION_DATE" =~ ^[0-9]{4}-[0-9]{2}-[0-9]{2}$ ]]; then
    echo -e "${RED}Error: Invalid expiration date format. Use YYYY-MM-DD (e.g., 2025-12-31)${NC}"
    exit 1
fi

# Check if private key exists
if [ ! -f "$PRIVATE_KEY" ]; then
    echo -e "${RED}Error: Private key file not found: $PRIVATE_KEY${NC}"
    echo "Generate keys first with: $0 --generate-keys"
    exit 1
fi

# Check if license_generator executable exists
LICENSE_GEN=""
# Try multiple possible locations
POSSIBLE_PATHS=(
    "./license_generator"
    "build/tools/license_generator"
    "build/bin/license_generator"
    "../build/tools/license_generator"
    "../build/bin/license_generator"
    "$(dirname "$0")/../build/tools/license_generator"
    "$(dirname "$0")/../build/bin/license_generator"
)

for path in "${POSSIBLE_PATHS[@]}"; do
    if [ -f "$path" ] && [ -x "$path" ]; then
        LICENSE_GEN="$path"
        break
    fi
done

if [ -z "$LICENSE_GEN" ]; then
    echo -e "${RED}Error: license_generator executable not found!${NC}"
    echo "Build it first:"
    echo "  cd build"
    echo "  cmake .. -DCVEDIX_WITH_LICENSE=ON"
    echo "  make license_generator"
    exit 1
fi

# Generate license
echo -e "${GREEN}Generating license...${NC}"
echo "  Private key:  $PRIVATE_KEY"
echo "  Output file:  $OUTPUT_LICENSE"
echo "  Expiration:   $EXPIRATION_DATE"
echo "  Features:     $FEATURES"
echo ""

"$LICENSE_GEN" "$PRIVATE_KEY" "$OUTPUT_LICENSE" "$EXPIRATION_DATE" "$FEATURES"

if [ $? -eq 0 ]; then
    echo ""
    echo -e "${GREEN}✓ License generated successfully!${NC}"
    echo ""
    echo "Next steps:"
    echo "  1. Copy license file to target system:"
    echo "     cp $OUTPUT_LICENSE /opt/cvedix/license.lic"
    echo ""
    echo "  2. Copy public key to target system:"
    echo "     cp $PUBLIC_KEY /opt/cvedix/public_key.pem"
    echo ""
    echo "  3. Or set environment variables:"
    echo "     export CVEDIX_LICENSE_PATH=$OUTPUT_LICENSE"
    echo "     export CVEDIX_LICENSE_PUBLIC_KEY=$PUBLIC_KEY"
    echo ""
else
    echo -e "${RED}✗ Failed to generate license${NC}"
    exit 1
fi

