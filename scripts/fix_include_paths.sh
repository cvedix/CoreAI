#!/bin/bash
# Script to fix include paths from relative (../objects/) to absolute (cvedix/objects/)

set -e

cd "$(dirname "$0")/.."

echo "Fixing include paths in nodes/ directory..."

# Function to fix include paths in a file
fix_file() {
    local file="$1"
    local dir=$(dirname "$file")
    local depth=$(echo "$dir" | tr -cd '/' | wc -c)
    
    # Calculate relative depth from nodes/ root
    if [[ "$dir" == nodes/* ]]; then
        depth=$(echo "$dir" | sed 's|nodes/||' | tr -cd '/' | wc -c)
    fi
    
    # Fix ../objects/ -> cvedix/objects/
    sed -i 's|#include "\.\./objects/|#include "cvedix/objects/|g' "$file"
    sed -i 's|#include "\.\./\.\./objects/|#include "cvedix/objects/|g' "$file"
    sed -i 's|#include "\.\./\.\./\.\./objects/|#include "cvedix/objects/|g' "$file"
    
    # Fix ../utils/ -> cvedix/utils/
    sed -i 's|#include "\.\./utils/|#include "cvedix/utils/|g' "$file"
    sed -i 's|#include "\.\./\.\./utils/|#include "cvedix/utils/|g' "$file"
    sed -i 's|#include "\.\./\.\./\.\./utils/|#include "cvedix/utils/|g' "$file"
    
    # Fix ../excepts/ -> cvedix/excepts/
    sed -i 's|#include "\.\./excepts/|#include "cvedix/excepts/|g' "$file"
    sed -i 's|#include "\.\./\.\./excepts/|#include "cvedix/excepts/|g' "$file"
    sed -i 's|#include "\.\./\.\./\.\./excepts/|#include "cvedix/excepts/|g' "$file"
}

# Find all .h and .cpp files in nodes/
find nodes -type f \( -name "*.h" -o -name "*.cpp" \) | while read -r file; do
    if grep -q '#include "\.\./' "$file"; then
        echo "Fixing: $file"
        fix_file "$file"
    fi
done

echo "Done fixing include paths!"

