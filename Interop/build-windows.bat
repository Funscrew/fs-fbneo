echo off

rem cmake -S . -B build
msbuild /p:Configuration=Debug  build\NetcodeInterop.vcxproj

rem Copy Libs to C# project...
XCOPY .\build\Debug .\CSharpExample\bin\Debug\net9.0 /E /Y /I /D
