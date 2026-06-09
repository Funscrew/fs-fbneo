rm -rf build
mkdir build
mkdir build/debug
mkdir build/release

cmake -S . -B build/debug 
cmake -S . -B build/release

cmake --build build/debug
cmake --build build/release
