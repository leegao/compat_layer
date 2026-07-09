sh build_linux_x86_64.sh
cd build
make install
cd -

sh build_linux_i86.sh
cd build_i86
make install
cd -

echo "COMPAT_LOG_LEVEL=info,error ENABLE_DXVK_MALI_COMPAT_LAYER=1 VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation vkcube"
