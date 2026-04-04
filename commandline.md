# FS-FBNEO Command Line Options

## General
_--rom&lt;name&gt;_  
This is the ROM that will be loaded, using the given name


## Multiplayer
### Direct Connections
You can connect directly to another client to play a game.  Both players must use the same _ROM name_ and compatible IP/Port numbers.

```
// This will load the 3rd strike ROM and attempt to connect to another player at IP 1.2.3.4 and port 7001.  The port they should connect to is 7000.
fsfbneo.exe --rom sfiii3nr1 direct -l localhost:7000 -r 1.2.3.4:7001 --player 1 --name "My Name" --delay 1

// The other player would use the command line, assuming that our IP address is 5.6.7.8
fsfbneo.exe --rom sfiii3nr1 direct -l localhost:7001 -r 5.6.7.8:7000 --player 1 --name "My Name" --delay 1
```

### With Replay Appliance
ReplayAppliance can be used to send game replay data to a remote server for playback later.
Use --s <ip:port> to specify the address/port of the replay appliance.
Use --i <replayid> to specify the replay id.  Each replay id should be unique.

Here is an example command line of connecting to a ReplayAppliance:
```
--rom sfiii3nr1 direct -l 127.0.0.1:7000 -r 127.0.0.1:7001 --player 2 --name "Screwie" --delay 0 -s 127.0.0.1:7002 -i 12345
```


## Debugging - Quickstart
Install a copy of fightcade + any ROMS you want to work with if you haven't already.  Then, copy the content from __&lt;fightcadeinstalldirectory&gt;emulator\fbneo__ to the folder: __&lt;fs-fbneo-repo-directory&gt;projectfiles\visualstudio-2022\Debug__.

You will now be able to load and run any ROMS that you want to debug against.