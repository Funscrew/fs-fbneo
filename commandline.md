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
--rom sfiii3nr1 direct -l 127.0.0.1:7000 -r 127.0.0.1:7001 --player 2 --name "Screwie" --delay 1 -s 127.0.0.1:7002 -i 12345
```

#### C# Args for ReplayAppliance:
```
```


### Test EchoClient
```
--rom sfiii3nr1 direct -l 127.0.0.1:7001 -r 127.0.0.1:7000 --player 2 --name "Archie" --delay 1
```

other stuff...
```
rem call dotnet build -c Debug ReplayAppliance\ReplayAppliance.csproj
call ReplayAppliance\bin\Debug\net8.0\ReplayAppliance.exe input-echo --game-name "sfiii3nr1" --player 1 --name "Echoman" --replay-options "127.0.0.1:7002" --session-id "12345"

rem call dotnet build -c Debug ReplayAppliance\ReplayAppliance.csproj
call ReplayAppliance\bin\Debug\net8.0\ReplayAppliance.exe replay-appliance --session-id "12345" --local-port 7002 --game-name "sfiii3rn1" --game-version "0.01"

```



## Debugging - Quickstart
Install a copy of fightcade + any ROMS you want to work with if you haven't already.  Then, copy the content from __&lt;fightcadeinstalldirectory&gt;emulator\fbneo__ to the folder: __&lt;fs-fbneo-repo-directory&gt;projectfiles\visualstudio-2022\Debug__.

You will now be able to load and run any ROMS that you want to debug against.




## Test : Echo Client + fs-fbneo
C#
input-echo --game-name "sfiii3nr1" --player 1 --name "Joe" --local-port 7000 --session-id 12345 --remote "127.0.0.1:7001-2"

C++
--rom sfiii3nr1 direct -l 127.0.0.1:7001 -r 127.0.0.1:7000 --player 2 --name "Archie" --delay 1



## Test: Echo Client + fs-fbneo + ReplayAppliance
Start the replay appliance in test mode first!

C# 
input-echo --game-name "sfiii3nr1" --player 1 --name "Joe" --replay-options "127.0.0.1:7002" --session-id "12345" --local-port 7000 --remote "127.0.0.1:7001-2"

C++
--rom sfiii3nr1 direct -l 127.0.0.1:7001 -r 127.0.0.1:7000 --player 2 --name "Archie" --delay 1 -s 127.0.0.1:7002 -i 12345




input-echo --game-name "sfiii3nr1" --player 1 --name "Joe" --session-id "12345" --local-port 7000 --remote "127.0.0.1:7001-2"



## Manual Test Scenarios:

### ONE:
Conditions:
- fs-fbneo client connecting to unreachable replay appliance:
- EchoClient : Connecting to other player, NO replay appliance connection listed.

Command Line Args:
fs-fbneo: 
```
--rom sfiii3nr1 direct -l 127.0.0.1:7001 -r 127.0.0.1:7000 --player 2 --name "Archie" --delay 1 -s 127.0.0.1:7002 -i 12345
```
ReplayAppliance:
```
input-echo --game-name "sfiii3nr1" --player 1 --name "Joe" --session-id "12345" --local-port 7000 --remote "127.0.0.1:7001-2"
```


#### Tests:
1. Start fsfbneo client first, then start EchoClient. -- fsfbneo client replay appliance connection will time out after configured time (~5 seconds), and games will sync + operate as normal.
2. Start EchoCLient first, then start fs-fbneo.  -- Same expected results as above.  After timeout period on fs-fbneo side, the games will sync + operate as normal.

## TWO:
Same as test ONE, but this time EchoClient attempts to connect to the ReplayAppliance, but fs-fbneo does not.

Command Line Args:
fs-fbneo: 
```
--rom sfiii3nr1 direct -l 127.0.0.1:7001 -r 127.0.0.1:7000 --player 2 --name "Archie" --delay 1
```
EchoClient (via ReplayAppliance):
```
input-echo --game-name "sfiii3nr1" --player 1 --name "Joe" --session-id "12345" --local-port 7000 --remote "127.0.0.1:7001-2"
```

#### Tests:
1. Start fs-fbneo client first, then start EchoClient. -- EchoClient replay appliance connection will time out after configured time (~5 seconds), and games will sync + operate as normal.
2. Start EchoCLient first, then start fs-fbneo.  -- Same expected results as above.  After timeout period on EchoClient side, the games will sync + operate as normal.



### THREE
Connecting to actual ReplayAppliance server.  This combines the above two scenarios
but this time they are allowed to connect to the server, and the game session should be recorded.

Command Line Args:
ReplayAppliance:
```
replay-appliance --service-port 5000 --replay-port 7002  --use-test-session
```


FS-FBNEO:
```
--rom sfiii3nr1 direct -l 127.0.0.1:7001 -r 127.0.0.1:7000 --player 2 --name "Archie" --delay 1 -s 127.0.0.1:7002 -i 12345
```

Echo Client:
```
input-echo --game-name "sfiii3nr1" --player 1 --name "Joe" --session-id "12345" --local-port 7000 --remote "127.0.0.1:7001-2" --replay-options "127.0.0.1:7002"
```

