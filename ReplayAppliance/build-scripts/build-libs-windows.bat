pushd "..\..\Interop"

call build-windows.bat

xcopy "build\Debug" "..\ReplayAppliance\libs\Debug" /E /I /Y /D
xcopy "build\Release" "..\ReplayAppliance\libs\Release" /E /I /Y /D

echo
echo BUILD OK

popd

echo
echo "ALL DONE"
