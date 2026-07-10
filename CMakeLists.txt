cmake_minimum_required(VERSION 3.16)
project(dxvk_mali_compat_layer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT DEFINED CMAKE_INSTALL_PREFIX)
    set(CMAKE_INSTALL_PREFIX "/usr") # or ~/.local/
endif()

set(JSON_INSTALL_DIR "share/vulkan/implicit_layer.d")
if(NOT DEFINED LIB_INSTALL_DIR)
    set(LIB_INSTALL_DIR "lib/x86_64-linux-gnu")
endif()

if(NOT DEFINED JSON_LIBRARY_PATH)
    if(NOT "${LIB_INSTALL_DIR}" STREQUAL "lib/x86_64-linux-gnu")
        set(JSON_LIBRARY_PATH "./")
    else()
        set(JSON_LIBRARY_PATH "")
    endif()
endif()

if(NOT DEFINED JSON_LIBRARY32_PATH)
    if(NOT "${LIB_INSTALL_DIR}" STREQUAL "lib/x86_64-linux-gnu")
        set(JSON_LIBRARY32_PATH "./")
    else()
        set(JSON_LIBRARY32_PATH "")
    endif()
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

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")

add_library(dxvk_mali_compat_layer SHARED ${SOURCES})

# # libGPUCounters
# if(CMAKE_SIZEOF_VOID_P EQUAL 8
#     AND (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "arm64" OR ANDROID_ABI MATCHES "arm64-v8a")
# )
#     set(HWCPIPE_WERROR OFF CACHE BOOL "" FORCE)

#     add_subdirectory(third_party/libGPUCounters)

#     # all_gpu_counters.cpp gets stuck on -O3, so let's compile it separately at -O0
#     add_library(all_gpu_counters_obj OBJECT
#         "third_party/libGPUCounters/hwcpipe/src/hwcpipe/all_gpu_counters.cpp"
#     )
#     target_compile_options(all_gpu_counters_obj PRIVATE "-O0")
#     target_include_directories(all_gpu_counters_obj PRIVATE
#         "third_party/libGPUCounters/hwcpipe/include"
#         "third_party/libGPUCounters/hwcpipe/src"
#         "third_party/libGPUCounters/backend/device/include"
#     )
#     set_target_properties(all_gpu_counters_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
#     target_compile_definitions(all_gpu_counters_obj PRIVATE ANDROID)
#     get_target_property(HWCP_SOURCES hwcpipe SOURCES)
#     list(REMOVE_ITEM HWCP_SOURCES "src/hwcpipe/all_gpu_counters.cpp")
#     set_target_properties(hwcpipe PROPERTIES SOURCES "${HWCP_SOURCES}")
#     target_sources(hwcpipe PRIVATE $<TARGET_OBJECTS:all_gpu_counters_obj>)

#     # third_party/libGPUCounters/backend/device/include/device/handle.hpp:90:30: error: ‘uint32_t’ has not been declared
#     #   90 |     static handle_ptr create(uint32_t instance_number = 0);
#     #                                     ^~~~~~~~
#     if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
#         target_compile_options(device PRIVATE "-include" "cstdint")
#     endif()
#     target_link_libraries(dxvk_mali_compat_layer PRIVATE
#         device
#         hwcpipe
#     )

#     target_compile_definitions(dxvk_mali_compat_layer PRIVATE HAS_LIBGPUCOUNTERS=1)
# endif()

target_compile_definitions(dxvk_mali_compat_layer PRIVATE ENABLE_COMPUTE_TRACKING=1)
set_target_properties(dxvk_mali_compat_layer PROPERTIES
    PREFIX "lib"
    OUTPUT_NAME "${OUT_NAME}"
)

target_include_directories(dxvk_mali_compat_layer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/src
)

install(TARGETS dxvk_mali_compat_layer DESTINATION ${LIB_INSTALL_DIR})
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libdxvk_mali_compat_layer.json" DESTINATION ${JSON_INSTALL_DIR})
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libdxvk_mali_compat_layer32.json" DESTINATION ${JSON_INSTALL_DIR})
