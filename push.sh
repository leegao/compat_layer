push() {
	adb push $1 /data/local/tmp/
	adb shell run-as com.ludashi.benchmark "cp /data/local/tmp/$1 files/imagefs/usr/lib/$1"
	adb shell run-as com.ludashi.benchmark "ls -lh files/imagefs/usr/lib/$1"
}

push_vvl() {
	adb push $1 /data/local/tmp/
	adb shell run-as com.ludashi.benchmark "cp /data/local/tmp/$1 files/imagefs/usr/share/vulkan/explicit_layer.d/$1"
	adb shell run-as com.ludashi.benchmark "ls -lh files/imagefs/usr/share/vulkan/explicit_layer.d/$1"
}

sh compile_android.sh
cd build_arm64
push libdxvk_mali_compat_layer.so
cd -

push_vvl libdxvk_mali_compat_layer.json

echo VK_INSTANCE_LAYERS=VK_LAYER_COMPAT_DxvkMaliCompatLayer
