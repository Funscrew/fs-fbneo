echo off

rmdir /S /Q "build"

cmake -G "Visual Studio 17 2022" -S . -B build
rem cmake -S . -B build
cmake --build build --config Debug
cmake --build build --config Release

XCOPY .\build\Debug ..\ReplayAppliance\libs\Debug /E /I /Y /D
XCOPY .\build\Release ..\ReplayAppliance\libs\Release /E /I /Y /D

rem msbuild /p:Configuration=Debug    build\NetcodeInterop.vcxproj
rem msbuild /p:Configuration=Release  build\NetcodeInterop.vcxproj

rem Copy Libs to C# project...
rem XCOPY .\build\Debug .\CSharpExample\bin\Debug\net9.0 /E /Y /I /D
