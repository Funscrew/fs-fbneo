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

<strike>
**12)** If you want to test a new detector, put it in '*build\detector*'.
</strike>


## How To Run

### GGPO Logging
GGPO logging can be activated with the following CLI option:
```
--logf "<pathtolog>;[active,categories]"
```
Provide the path to where you want the log to be written, and optionally a comma delimited list of all of the categories that you want to include.  If omitted, all categories will be included  For example:
```
--logf "ggpo.log"
```
Will write all categories to the file: 'ggpo.log'

```
--logf "ggpo.log;UDP,MSG,NET"
```
Will only include the **UDP**, **MSG**, and **NET** categories.

Logging is intended to help with development, tracking of statistics, and to help run down certain bugs.  The logs themselves may not be 100% human readable in an effort to minimize their size, and system resources required while a game is running.


All available categories are defined in log.h, as seen below:

```
static const char* CATEGORY_GENERAL = "NA";
static const char* CATEGORY_MESSAGE = "MSG";
static const char* CATEGORY_ENDPOINT = "EP";
static const char* CATEGORY_EVENT = "EVT";
static const char* CATEGORY_SYNC = "SYNC";
static const char* CATEGORY_RUNNING = "RUN";
static const char* CATEGORY_CONNECTION = "CONN";
static const char* CATEGORY_ERROR = "ERR";
static const char* CATEGORY_NETWORK = "NET";
static const char* CATEGORY_INPUT = "INP";
static const char* CATEGORY_TEST = "TEST";
static const char* CATEGORY_UDP = "UDP";
static const char* CATEGORY_INPUT_QUEUE = "INPQ";
static const char* CATEGORY_TIMESYNC = "TIME";
```
