echo off

rmdir /S /Q "build"

cmake -S . -B build
cmake --build build --config Debug
cmake --build build --config Release

rem msbuild /p:Configuration=Debug    build\NetcodeInterop.vcxproj
rem msbuild /p:Configuration=Release  build\NetcodeInterop.vcxproj

rem Copy Libs to C# project...
rem XCOPY .\build\Debug .\CSharpExample\bin\Debug\net9.0 /E /Y /I /D
