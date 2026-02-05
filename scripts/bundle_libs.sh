#!/bin/bash
set -e

APP_DIR="$1"

if [ -z "$APP_DIR" ]; then
    echo "Usage: $0 <AppDir>"
    exit 1
fi

LIB_DIR="$APP_DIR/usr/lib"
mkdir -p "$LIB_DIR"

echo "Bundling dependencies for $APP_DIR..."

# List of libraries to exclude (system libraries that should be on the host)
# This list prevents bundling glibc, kernel drivers, and basic system utils
EXCLUDE_LIST="libc.so|libpthread.so|libdl.so|libm.so|libstdc++.so|libgcc_s.so|librt.so|libz.so|libutil.so|ld-linux|libGL.so|libEGL.so|libdrm.so|libxcb"

# Function to copy a library and its real file if it's a symlink
copy_lib() {
    local lib_path="$1"
    local dest_dir="$2"
    local lib_name=$(basename "$lib_path")

    # Skip if already exists in valid form
    if [ -f "$dest_dir/$lib_name" ]; then
        return
    fi

    echo "  Bundling: $lib_name"
    cp -d "$lib_path" "$dest_dir/"

    # If it's a symlink, resolve and copy target
    if [ -L "$lib_path" ]; then
        local real_path=$(readlink -f "$lib_path")
        local real_name=$(basename "$real_path")
        
        # Only copy if different name (avoid self mutation if symlink is weird) and not already there
        if [ "$real_name" != "$lib_name" ]; then
             # Recurse slightly to handle chain of symlinks if needed, 
             # but usually copying the resolved real path is enough.
             # We put the real file in the same dir.
             if [ ! -f "$dest_dir/$real_name" ]; then
                echo "    + Real file: $real_name"
                cp "$real_path" "$dest_dir/"
             fi
        fi
    fi
}

# Find all executables and shared libraries in AppDir
# We look in usr/bin and standard lib locations. 
# Depending on build, main binaries might be in usr/bin
BINARIES=$(find "$APP_DIR/usr/bin" -type f -executable)
LIBRARIES=$(find "$APP_DIR/usr/lib" -name "*.so*" -type f)

# Process list
for BIN in $BINARIES $LIBRARIES; do
    # Skip if not an elf file (scripts, etc)
    if ! file "$BIN" | grep -q "ELF"; then
        continue
    fi

    # Use ldd to find dependencies
    # Output format: "libname.so => /path/to/libname.so (0x...)"
    # We grep for "=>" to get resolved libs
    DEPENDENCIES=$(ldd "$BIN" 2>/dev/null | grep "=>" | awk '{print $3}' | grep "^/")

    for DEP in $DEPENDENCIES; do
        LIB_NAME=$(basename "$DEP")
        
        # Check exclusion list
        if echo "$LIB_NAME" | grep -qE "$EXCLUDE_LIST"; then
            continue
        fi

        # Check if it's already in the AppDir structure (e.g. built by cmake install)
        # However, checking existence in $LIB_DIR alone isn't enough if the structure has subdirs
        # For simplicity in this script, we flatten deps into usr/lib or check if they are already there.
        # But wait, we want to copy them IF they are system libs we want to bundle.
        # If the lib is INSIDE AppDir already (processed by make install), we don't need to copy it from system.
        
        if [[ "$DEP" == "$APP_DIR"* ]]; then
            continue
        fi

        copy_lib "$DEP" "$LIB_DIR"
    done
done

echo "Dependency bundling complete."
