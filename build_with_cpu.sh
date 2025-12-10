mkdir build
cd build
cmake -DCVEDIX_WITH_GSTREAMER=ON 
      -DCVEDIX_WITH_LLM=ON \
      -DCVEDIX_WITH_FFMPEG=ON \
      -DCVEDIX_BUILD_SAMPLES=ON \
      -DCVEDIX_WITH_LICENSE=ON \
      ..
make -j$(nproc)