if [ -z "${ANDROID_NDK_HOME}" ]; then
    echo "Error: ANDROID_NDK_HOME is not set." >&2
    echo "    Try $HOME/Android/Sdk/ndk/" >&2
    exit 1
fi

cmake -B build_arm64 \
    -DCMAKE_SYSTEM_NAME=Android \
    -DANDROID_PLATFORM=26 \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_TOOLCHAIN=clang \
    -DANDROID_STL=c++_static \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake" \
    -DCMAKE_WARN_DEPRECATED=OFF \

cmake --build build_arm64
