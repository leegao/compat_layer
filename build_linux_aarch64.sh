cmake -B build_aarch64_glibc \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
  -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
  -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ \
  -DCMAKE_INSTALL_PREFIX=$HOME/.local \
  -DLIB_INSTALL_DIR="share/vulkan/implicit_layer.d" \
  -DJSON_INSTALL_DIR="share/vulkan/implicit_layer.d" \
  -DBUILD_VARIANT=linux-aarch64
  # -DCMAKE_C_FLAGS="-fsanitize=thread -g" \
  # -DCMAKE_CXX_FLAGS="-fsanitize=thread -g"

cmake --build build_aarch64_glibc
