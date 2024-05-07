@echo off
for %%i in (Assets\Shaders\*.vert Assets\Shaders\*.frag) do "C:\VulkanSDK\1.3.261.1\Bin\glslc.exe" "%%~i" -o "%%~i.spv"
pause