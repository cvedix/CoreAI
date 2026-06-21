mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCVEDIX_WITH_GSTREAMER=ON \
      -DCVEDIX_WITH_LLM=OFF \
      -DCVEDIX_BUILD_SAMPLES=ON \
      ..
make -j$(nproc)