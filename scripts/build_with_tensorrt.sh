mkdir build
cd build
cmake -DCVEDIX_WITH_GSTREAMER=ON \
      -DCVEDIX_WITH_LLM=ON \
      -DCVEDIX_WITH_CUDA=ON \
      -DCVEDIX_WITH_TRT=ON \
      -DCVEDIX_BUILD_SAMPLES=ON \
      ..
make -j$(nproc)