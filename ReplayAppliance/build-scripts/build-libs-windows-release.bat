pushd "..\..\Interop"

call build-windows.bat

xcopy "build\Release" "..\ReplayAppliance\ReplayAppliance\libs" /E /I /Y /D

echo
echo BUILD OK

popd

echo
echo "ALL DONE"
