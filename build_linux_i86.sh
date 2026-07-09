cmake -B build_i86 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_INSTALL_PREFIX=$HOME/.local \
  -DLIB_INSTALL_DIR="share/vulkan/implicit_layer.d" \
  -DJSON_INSTALL_DIR="share/vulkan/implicit_layer.d" \
  -DOUT_NAME="descriptor_buffer_layer32" \
  -DCMAKE_CXX_FLAGS="-m32" \
  -DCMAKE_SHARED_LINKER_FLAGS="-m32" \
  -DCMAKE_EXE_LINKER_FLAGS="-m32"

cmake --build build_i86
