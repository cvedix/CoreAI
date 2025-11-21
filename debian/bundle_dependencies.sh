#!/bin/bash
# Script to bundle dependencies into Debian package

set -e

SDK_LIB="$1"
TARGET_DIR="$2"

if [ -z "$SDK_LIB" ] || [ -z "$TARGET_DIR" ]; then
    echo "Usage: $0 <sdk_library_path> <target_directory>"
    exit 1
fi

if [ ! -f "$SDK_LIB" ]; then
    echo "Error: SDK library not found: $SDK_LIB"
    exit 1
fi

mkdir -p "$TARGET_DIR/opencv/core"
mkdir -p "$TARGET_DIR/opencv/optional"
mkdir -p "$TARGET_DIR/gstreamer"
mkdir -p "$TARGET_DIR/gtk"
mkdir -p "$TARGET_DIR/deps"

echo "Bundling dependencies for: $SDK_LIB"

# List of OpenCV core modules (required) - MUST be defined before use
OPENCV_CORE_MODULES=(
    "libopencv_core"
    "libopencv_imgproc"
    "libopencv_imgcodecs"
    "libopencv_videoio"
    "libopencv_dnn"
    "libopencv_highgui"
    "libopencv_features2d"
    "libopencv_calib3d"
    "libopencv_flann"
    "libopencv_objdetect"
)

# List of optional OpenCV modules that might not be available on target system
OPENCV_OPTIONAL_MODULES=(
    "libopencv_gapi"
    "libopencv_signal"
    "libopencv_xfeatures2d"
    "libopencv_stitching"
    "libopencv_superres"
    "libopencv_videostab"
)

# List of GStreamer libraries
GSTREAMER_LIBS=(
    "libgstrtspserver-1.0"
)

# List of GTK libraries (required for OpenCV highgui)
GTK_LIBS=(
    "libgtk-3"
    "libgdk-3"
    "libgdk_pixbuf-2.0"
    "libcairo-2"
    "libpango-1.0"
    "libpangocairo-1.0"
)

# Find and copy OpenCV core modules
echo "Bundling OpenCV core modules..."
for lib_name in "${OPENCV_CORE_MODULES[@]}"; do
    # Try to find in standard library paths using ldconfig
    lib_path=$(ldconfig -p 2>/dev/null | grep -E "${lib_name}\.so" | head -1 | awk '{print $NF}' || true)
    # If not found via ldconfig, try direct search in common paths
    if [ -z "$lib_path" ] || [ ! -e "$lib_path" ]; then
        lib_path=$(find /usr/local/lib /usr/lib /usr/lib/aarch64-linux-gnu /usr/lib/x86_64-linux-gnu -name "${lib_name}.so*" -type f 2>/dev/null | head -1 || true)
    fi
    if [ -n "$lib_path" ] && [ -e "$lib_path" ]; then
        echo "Found and bundling core: $lib_path"
        # Copy the symlink
        cp -a "$lib_path" "$TARGET_DIR/opencv/core/" 2>/dev/null || true
        # If it's a symlink, also copy the target
        if [ -L "$lib_path" ]; then
            real_path=$(readlink -f "$lib_path" 2>/dev/null || readlink "$lib_path" 2>/dev/null || true)
            if [ -n "$real_path" ] && [ -e "$real_path" ]; then
                cp -a "$real_path" "$TARGET_DIR/opencv/core/" 2>/dev/null || true
            fi
        fi
        # Copy all related .so files (versioned)
        lib_dir=$(dirname "$lib_path")
        lib_base=$(basename "$lib_path" | sed 's/\.so.*//')
        find "$lib_dir" -maxdepth 1 -name "${lib_base}.so*" -type f 2>/dev/null | while read -r found_lib; do
            if [ -n "$found_lib" ] && [ "$found_lib" != "$lib_path" ]; then
                cp -a "$found_lib" "$TARGET_DIR/opencv/core/" 2>/dev/null || true
            fi
        done
    else
        # Try to find in /usr/local/lib (common for source builds)
        find /usr/local/lib /usr/lib /usr/lib/aarch64-linux-gnu /usr/lib/x86_64-linux-gnu -name "${lib_name}.so*" -type f 2>/dev/null | head -3 | while read -r found_lib; do
            if [ -n "$found_lib" ] && [ -e "$found_lib" ]; then
                echo "Found and bundling core: $found_lib"
                cp -a "$found_lib" "$TARGET_DIR/opencv/core/" 2>/dev/null || true
            fi
        done
    fi
done

# Find and copy OpenCV optional modules
echo "Bundling OpenCV optional modules..."
for lib_name in "${OPENCV_OPTIONAL_MODULES[@]}"; do
    # Try to find in standard library paths using ldconfig
    lib_path=$(ldconfig -p 2>/dev/null | grep -E "${lib_name}\.so" | head -1 | awk '{print $NF}' || true)
    if [ -n "$lib_path" ] && [ -e "$lib_path" ]; then
        echo "Found and bundling optional: $lib_path"
        # Copy the symlink
        cp -a "$lib_path" "$TARGET_DIR/opencv/optional/" 2>/dev/null || true
        # If it's a symlink, also copy the target
        if [ -L "$lib_path" ]; then
            real_path=$(readlink -f "$lib_path" 2>/dev/null || readlink "$lib_path" 2>/dev/null || true)
            if [ -n "$real_path" ] && [ -e "$real_path" ]; then
                cp -a "$real_path" "$TARGET_DIR/opencv/optional/" 2>/dev/null || true
            fi
        fi
        # Copy all related .so files (versioned)
        lib_dir=$(dirname "$lib_path")
        lib_base=$(basename "$lib_path" | sed 's/\.so.*//')
        find "$lib_dir" -maxdepth 1 -name "${lib_base}.so*" -type f 2>/dev/null | while read -r found_lib; do
            if [ -n "$found_lib" ] && [ "$found_lib" != "$lib_path" ]; then
                cp -a "$found_lib" "$TARGET_DIR/opencv/optional/" 2>/dev/null || true
            fi
        done
    else
        # Try to find in /usr/local/lib (common for source builds) and standard paths
        find /usr/local/lib /usr/lib /usr/lib/aarch64-linux-gnu /usr/lib/x86_64-linux-gnu -name "${lib_name}.so*" -type f 2>/dev/null | head -3 | while read -r found_lib; do
            if [ -n "$found_lib" ] && [ -e "$found_lib" ]; then
                echo "Found and bundling optional: $found_lib"
                cp -a "$found_lib" "$TARGET_DIR/opencv/optional/" 2>/dev/null || true
            fi
        done
    fi
done

# Find and copy GStreamer RTSP server
for lib_name in "${GSTREAMER_LIBS[@]}"; do
    lib_path=$(ldconfig -p 2>/dev/null | grep -E "${lib_name}\.so" | head -1 | awk '{print $NF}' || true)
    if [ -n "$lib_path" ] && [ -e "$lib_path" ]; then
        echo "Found and bundling: $lib_path"
        # Copy the symlink
        cp -a "$lib_path" "$TARGET_DIR/gstreamer/" 2>/dev/null || true
        # If it's a symlink, also copy the target
        if [ -L "$lib_path" ]; then
            real_path=$(readlink -f "$lib_path" 2>/dev/null || readlink "$lib_path" 2>/dev/null || true)
            if [ -n "$real_path" ] && [ -e "$real_path" ]; then
                cp -a "$real_path" "$TARGET_DIR/gstreamer/" 2>/dev/null || true
            fi
        fi
        # Copy all related .so files (versioned)
        lib_dir=$(dirname "$lib_path")
        lib_base=$(basename "$lib_path" | sed 's/\.so.*//')
        find "$lib_dir" -maxdepth 1 -name "${lib_base}.so*" -type f 2>/dev/null | while read -r found_lib; do
            if [ -n "$found_lib" ] && [ "$found_lib" != "$lib_path" ]; then
                cp -a "$found_lib" "$TARGET_DIR/gstreamer/" 2>/dev/null || true
            fi
        done
    else
        find /usr/lib /usr/lib/aarch64-linux-gnu /usr/lib/x86_64-linux-gnu -name "${lib_name}.so*" -type f 2>/dev/null | head -3 | while read -r found_lib; do
            if [ -n "$found_lib" ] && [ -e "$found_lib" ]; then
                echo "Found and bundling: $found_lib"
                cp -a "$found_lib" "$TARGET_DIR/gstreamer/" 2>/dev/null || true
            fi
        done
    fi
done

# Find and copy GTK libraries (for OpenCV highgui)
echo "Bundling GTK libraries..."
for lib_name in "${GTK_LIBS[@]}"; do
    lib_path=$(ldconfig -p 2>/dev/null | grep -E "${lib_name}\.so" | head -1 | awk '{print $NF}' || true)
    if [ -z "$lib_path" ] || [ ! -e "$lib_path" ]; then
        lib_path=$(find /usr/lib /usr/lib/aarch64-linux-gnu /usr/lib/x86_64-linux-gnu -name "${lib_name}.so*" -type f 2>/dev/null | head -1 || true)
    fi
    if [ -n "$lib_path" ] && [ -e "$lib_path" ]; then
        echo "Found and bundling GTK: $lib_path"
        # Copy the symlink
        cp -a "$lib_path" "$TARGET_DIR/gtk/" 2>/dev/null || true
        # If it's a symlink, also copy the target
        if [ -L "$lib_path" ]; then
            real_path=$(readlink -f "$lib_path" 2>/dev/null || readlink "$lib_path" 2>/dev/null || true)
            if [ -n "$real_path" ] && [ -e "$real_path" ]; then
                cp -a "$real_path" "$TARGET_DIR/gtk/" 2>/dev/null || true
            fi
        fi
        # Copy all related .so files (versioned)
        lib_dir=$(dirname "$lib_path")
        lib_base=$(basename "$lib_path" | sed 's/\.so.*//')
        find "$lib_dir" -maxdepth 1 -name "${lib_base}.so*" -type f 2>/dev/null | while read -r found_lib; do
            if [ -n "$found_lib" ] && [ "$found_lib" != "$lib_path" ]; then
                cp -a "$found_lib" "$TARGET_DIR/gtk/" 2>/dev/null || true
            fi
        done
    else
        find /usr/lib /usr/lib/aarch64-linux-gnu /usr/lib/x86_64-linux-gnu -name "${lib_name}.so*" -type f 2>/dev/null | head -3 | while read -r found_lib; do
            if [ -n "$found_lib" ] && [ -e "$found_lib" ]; then
                echo "Found and bundling GTK: $found_lib"
                cp -a "$found_lib" "$TARGET_DIR/gtk/" 2>/dev/null || true
            fi
        done
    fi
done

echo "Dependency bundling completed."

