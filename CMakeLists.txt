cmake_minimum_required(VERSION 3.16)
project(dxvk_mali_compat_layer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT DEFINED CMAKE_INSTALL_PREFIX)
    set(CMAKE_INSTALL_PREFIX "/usr") # or ~/.local/
endif()

if(NOT DEFINED BUILD_VARIANT)
    set(BUILD_VARIANT "android-arm64")
endif()

if("${BUILD_VARIANT}" STREQUAL "android-arm64")
    set(JSON_LIBRARY32_PATH "./")
    set(JSON_LIBRARY_PATH "./")
elseif("${BUILD_VARIANT}" STREQUAL "linux-aarch64")
    set(JSON_LIBRARY32_PATH "./")
    set(JSON_LIBRARY_PATH "./")
else()
    set(JSON_LIBRARY32_PATH "")
    set(JSON_LIBRARY_PATH "")
endif()

set(JSON_INSTALL_DIR "share/vulkan/implicit_layer.d")
if(NOT DEFINED LIB_INSTALL_DIR)
    set(LIB_INSTALL_DIR "lib/x86_64-linux-gnu")
endif()

if(NOT DEFINED OUT_NAME)
    set(OUT_NAME dxvk_mali_compat_layer)
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/libdxvk_mali_compat_layer.json.in"
    "${CMAKE_CURRENT_BINARY_DIR}/libdxvk_mali_compat_layer.json"
    @ONLY
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/libdxvk_mali_compat_layer32.json.in"
    "${CMAKE_CURRENT_BINARY_DIR}/libdxvk_mali_compat_layer32.json"
    @ONLY
)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/spirv_reflect/*.c")

set(SPIRV_TOOLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/spirv-tools")

add_library(dxvk_mali_compat_layer SHARED ${SOURCES})

# Enable if the layer dispatches any compute shaders
# target_compile_definitions(dxvk_mali_compat_layer PRIVATE ENABLE_COMPUTE_TRACKING=1)

# Enable if the layer needs to track VkBuffers
target_compile_definitions(dxvk_mali_compat_layer PRIVATE ENABLE_BUFFER_TRACKING=1)

set_target_properties(dxvk_mali_compat_layer PROPERTIES
    PREFIX "lib"
    OUTPUT_NAME "${OUT_NAME}"
)

target_include_directories(dxvk_mali_compat_layer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/src
)

target_link_libraries(dxvk_mali_compat_layer PRIVATE
    "${SPIRV_TOOLS_DIR}/${BUILD_VARIANT}/libSPIRV-Tools-opt.a"
    "${SPIRV_TOOLS_DIR}/${BUILD_VARIANT}/libSPIRV-Tools.a"
)

target_include_directories(dxvk_mali_compat_layer PRIVATE
    "${SPIRV_TOOLS_DIR}/include"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_link_options(dxvk_mali_compat_layer PRIVATE "-Wl,--no-undefined")
endif()

install(TARGETS dxvk_mali_compat_layer DESTINATION ${LIB_INSTALL_DIR})
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libdxvk_mali_compat_layer.json" DESTINATION ${JSON_INSTALL_DIR})
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libdxvk_mali_compat_layer32.json" DESTINATION ${JSON_INSTALL_DIR})
