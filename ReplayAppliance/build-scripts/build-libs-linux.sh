pushd ../../Interop

source build-linux.sh

cp -r ./build/debug/libNetcodeInterop.so ../ReplayAppliance/libs/Debug
cp -r ./build/release/libNetcodeInterop.so ../ReplayAppliance/libs/Release

popd
