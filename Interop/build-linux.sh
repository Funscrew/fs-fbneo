rm -rf build
mkdir build
mkdir build/debug
mkdir build/release

cmake -S . -B build/debug 
cmake -S . -B build/release

cmake --build build/debug
cmake --build build/release

# TODO: Copy the files to libs directories....
cp -f ./build/debug/libNetcodeInterop.so ../ReplayAppliance/libs/Debug/libNetCodeInterop.so
cp -f ./build/release/libNetcodeInterop.so ../ReplayAppliance/libs/Release/libNetCodeInterop.so