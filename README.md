## Funscrew-FBNEO
FinalBurn Neo emulator built with FBNeo and an open-source GGPO implementation.

## FinalBurn Neo
Official Forum: https://neo-source.com

## How to Compile
NOTE: You will need at least VS2022 to build fs-FBNeo.

**1)** Download, install and run Visual Studio IDE Community Edition from https://visualstudio.microsoft.com/.

**2)** Download and install the 2010 DirectX SDK from https://www.microsoft.com/en-gb/download/details.aspx?id=6812). Make sure that you install it to the default location. The installer may also error out at the end, you can ignore it.

**3)** Go to '*projectfiles\visualstudio-2022*' and open '*fbneo_vs2022.sln*' in Visual Studio.

**4)** Select development with C++ if you are asked.

**5)** If you are asked to change the C++ toolchain, download and install the one it is asking for.

<strike>**6)** Install NASM for the assembly files from https://www.nasm.us/.
You need to put NASM.EXE somewhere in your PATH. You can put it in your Windows directory (IE: *'C:\Windows*'), but I do not recommend this for a number of reasons.</strike>  
**NOTE:**  It appears that NASM is no longer required.  Please reach out if you find that it is required...

**7)** Make sure the build settings are set to '*Release, x86*'. Debug, x86 configuration also builds correctly, but is intended for developers and debugging sessions.

**8)** Build using '*Build / Build Solution*' Menu (or using keyboard shortcuts, depending on your setup, F6, F7 or Ctrl+Shift+B).

**9)** Run '*fcadefbneo.exe*' that was compiled in the 'build' directory, or you can use '*Debug*' it from Visual Studio, it will launch it from that folder (debug version is '*fcadefbneod.exe*'


## NOTES ON OpenGL ##
I think that we need this lib:
https://sourceforge.net/projects/glew/files/glew/1.5.4/
--> We probably want the win32 version since we are doing and x86 build of the emulator at this time.

- I put the 1.5.4 version into src/dep/libs/glew-1.5.4 --> The license says that it is OK to redistribute, so we will!
- glew32*s*.lib is the static library, which is what we want, always!
--> Because we are using the static library, the GLEW_STATIC preprocessor is also defined:
--> https://stackoverflow.com/questions/49741322/glew-why-should-i-define-glew-static
--> Some jibberjabber about this is located in 'glew.h:175' which is an odd place to mention it.  I'm just glad that I was able to find the solution without having to pester ChatGPT.  Probably because GLEW linking problems aren't a hot topic on DDG or google for that matter :p

<strike>
**12)** If you want to test a new detector, put it in '*build\detector*'.
</strike>