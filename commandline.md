# FS-FBNEO Command Line Options

## General
_--rom&lt;name&gt;_  
This is the ROM that will be loaded, using the given name


## Multiplayer
### Direct Connections
You can connect directly to another client to play a game.  Both players must use the same _ROM name_ and compatible IP/Port numbers.

```
fsfbneo.exe --rom sf3iiinr1 direct -l localhost:7000 -r 123.1.2.3:7001 --player 1 --name "My Name" --delay 1
```