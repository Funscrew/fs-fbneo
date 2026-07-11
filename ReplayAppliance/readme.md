# ReplayAppliance
ReplayAppliance is the server application that is used to record P2P game sessions from fs-fbneo.
It is a C# application that uses interop libs from the emulator.
It builds and runs on Windows, Linux, and maybe MacOS? (untested)

ReplayAppliance was adapted from the experimental code GGPOSharp, which contains a direct, mostly compatible C# port of the GPPO code from fs-fbneo.
Future iterations of ReplayAppliance will get its netcode implementation directly from updated interop libs.


## Building the Interop Libs!
The application and test code contain a set of libs for Debug and Release builds, however, if you need to experiment with new features, fix bugs, etc. you will need to
rebuild the libs as you make updates.
There are a set of scripts included for the platform of your choice and they are located in **./build-scripts**.

**NOTE:** You do not need to build these unless you make changes.

```
# Choose one!
build-windows.bat
build-linux.sh

# build-macos.sh      # MAYBE NEVER?
```

These scripts copy the library files to **./ReplayAppliance/libs** and they will be copied to the correct location on build via targets defined in the .csproj file(s).



## Building Replay Appliance
Replay Appliance uses .net 9.0, you will need to install the SDK + Runtime on your machine before you are able to build it.

### Install .NET on Windows
Download and install the .net SDK from the following location:  
https://dotnet.microsoft.com/en-us/download/dotnet/9.0

This page includes information for other operating systems too.

### Install .NET On Linux
Linux is a bit more difficult because of the package managers.  On Ubuntu one would do the following:

```
sudo add-apt-repository ppa:dotnet/backports
sudo apt-get install -y dotnet-sdk-9.0
```

More information and other installation information can be found here:  
https://learn.microsoft.com/en-us/dotnet/core/install/linux-ubuntu-install?tabs=dotnet9&pivots=os-linux-ubuntu-2404


### Build the Application
Whatever your platform is, the application is built the same way:
```
# Debug
dotnet publish -c Debug -o "build"

# Release
dotnet publish -c Release -o "build"
```
You can even cross compile if you need to:
```
# This would build the application for use on a Linux machine, even if you are currently on Windows.
dotnet publish -c Release -o "build" -r linux-x64
```

<br />
<br />
<br />
<br />
<br />
<br />
<br />
