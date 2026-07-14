cmake -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_INSTALL_PREFIX=$HOME/.local \
  -DLIB_INSTALL_DIR="share/vulkan/implicit_layer.d" \
  -DJSON_INSTALL_DIR="share/vulkan/implicit_layer.d"
  # -DCMAKE_C_FLAGS="-fsanitize=thread -g" \
  # -DCMAKE_CXX_FLAGS="-fsanitize=thread -g"

cmake --build build
