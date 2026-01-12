#!/bin/bash
set -e

# Configuration
BUILD_DIR="build_appimage"
APP_DIR="AppDir"
OUTPUT_NAME="CvedixRuntime-x86_64.AppImage"

echo "Starting AppImage build process..."

# 1. Clean and Configure
rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR
cd $BUILD_DIR

echo "Configuring CMake..."
# Enable all backends to build all plugins (if deps are present)
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCVEDIX_WITH_RKNN=ON \
    -DCVEDIX_WITH_TRT=ON \
    -DCVEDIX_WITH_GSTREAMER=ON \
    -DCMAKE_INSTALL_PREFIX=/usr

# 2. Build
echo "Building..."
make -j$(nproc)

# 3. Install to AppDir
echo "Installing to AppDir..."
make install DESTDIR=../$APP_DIR

cd ..

# 4. Prepare AppDir Metadata
mkdir -p $APP_DIR/usr/bin
mkdir -p $APP_DIR/usr/lib
mkdir -p $APP_DIR/usr/share/applications
mkdir -p $APP_DIR/usr/share/icons

# Create desktop file
cat > $APP_DIR/usr/share/applications/cvedix-runtime.desktop <<EOF
[Desktop Entry]
Type=Application
Name=Cvedix Runtime
Exec=cvedix_launcher
Icon=cvedix
Categories=Development;
EOF

# Create AppRun
cat > $APP_DIR/AppRun <<EOF
#!/bin/bash
HERE="\$(dirname "\$(readlink -f "\${0}")")"
export LD_LIBRARY_PATH="\${HERE}/usr/lib:\${HERE}/usr/lib/cvedix/plugins:\$LD_LIBRARY_PATH"
export GST_PLUGIN_PATH="\${HERE}/usr/lib/gstreamer-1.0:\$GST_PLUGIN_PATH"

# Run the launcher or the argument
if [ -z "\$1" ]; then
    exec "\${HERE}/usr/bin/cvedix_launcher"
else
    exec "\$@"
fi
EOF

chmod +x $APP_DIR/AppRun

# 5. Copy Dependencies (Simplified - normally use linuxdeploy)
# For this demo, we assume system libs. In production, copy libopencv*, librknnrt*, etc.
echo "Copying plugins to standard location..."
# Plugins are installed to /usr/lib/cvedix/plugins by CMake, so they should be in AppDir/usr/lib/cvedix/plugins

echo "Running automatic dependency bundling..."
chmod +x ../scripts/bundle_libs.sh
../scripts/bundle_libs.sh "$APP_DIR"


# 6. Create AppImage with appimagetool if available
if command -v appimagetool &> /dev/null; then
    echo "Generating AppImage..."
    appimagetool $APP_DIR $OUTPUT_NAME
    echo "Success! Created $OUTPUT_NAME"
else
    echo "appimagetool not found. AppDir is ready at $APP_DIR"
    echo "You can run it via: ./$APP_DIR/AppRun"
fi
