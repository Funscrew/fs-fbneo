# ReplayAppliance
ReplayAppliance is the server application that is used to record P2P game sessions from fs-fbneo.
It is a C# application that uses interop libs from the emulator.
It builds and runs on Windows, Linux, and maybe MacOS? (untested)

ReplayAppliance was adapted from the experimental code GGPOSharp, which contains a direct, mostly compatible C# port of the GPPO code from fs-fbneo.
Future iterations of ReplayAppliance will get its netcode implementation directly from updated interop libs.


## Build It!
Before you run the application, you will need to build the interop libraries:
The scripts are located in **build-scripts** and there is one for each supported platform.
Generate the flavor that is appropriate for your current task.

**NOTE:** ReplayAppliance will not build if the libs aren't in the correct location.  This is on purpose as it allows flexibility when testing / debugging the application.  You are welcome to build + copy the libs by hand if you prefer.

```
# Choose one!

build-windows.bat
. build-linux.sh      # NOT YET
. build-macos.sh       # MAYBE NEVER?
```

These scripts copy the lib files to the ReplayAppliance project which will then copy them to the correct location on build.




<br />
<br />
<br />
<br />
<br />
<br />
<br />


## LEGACY README BELOW:
Leaving this here for now as the notes were useful at one point in time.


## Flow:
Each frame gets + syncs inputs + messages.

- If the simulation won't advance (for time reasons), run an idle routine.  This will run the polling + associated updates.

- If it will advance, then 'IncrementFrame' can be called.
--> This does:
1. Poll with no delay.
2. Read local inputs
3. --> add local inputs to client.
4. Sync the inputs
--> Callback for rollbacks, etc. will take place here.

From here the 'game' can run its next frame since it has synced inputs for all players.

NOTE: Any received message is rejected unless the sync event has completed.  This sets the internal 'remote magic' number
value to that of the sync.  Since all messages have this value (and it is the same) we can reject any non-sync related messages.


## Bugs
- BUG: We are not sending the local player name to the remote. (it is being set twice...)
- BUG: We are double-setting the local player number during client init.
--> This is actually more of a quality thing....


## Next Steps
 - I think the next best step (after another tweak or two) is to get the C++ client working and talking to the echo client again.  Then I can add in the replay endpoint stuff on that end just to confirm that everything is going to work in a 'live' scenario...
- Do some testing with OO (out of order) packets.  They should be handled just fine, but it would be good to confirm this....


# GGPO PROTO:


## Notes:


## STEPS:
# Synchronize
###  Running:
Clients are now exchanging packets.  Inspection of those packets 
Sync / Handshake:
During sync operations, each client sends a sync request, and expects a response from the endpoint.
For robustness, a certain number (default = 5) of successful sync replies are required before the clients will be considered synchronized.

## Polling and Events:
Each of the clients / backends poll each game frame.  This is where messages are send, received, and handled.  Events are created for processing in the next step.
After the polling step, the client will handle any events that were put the in the queue.




## GGPO Sepcific (C++)
- Each of the backends has a list of 'endpoints' that it communicates with.  This is the equivalent of one of the C# GGPOClient classes.
- Each endpoint is initialized with UDP connection information and a *poll manager*.
- Each of the endpoints share the same poll manager instance.
- Both UdpProtocol and Udp (the main UDP client wrapper) use the IPollSink interface, and so each of them get called in PollManager::Pump()
-- Udp is initialized when the backend is setup, and UdpProto is initialized when each of the players (endpoints) are added.  This means that Udp will always poll first
and this is where the network data (messages) comes from.  It makes sense to do it first, IMO....
-- Each backend has its own Udp instance, so only the poll manager, which fires off the callbacks, is shared between everything.  Personally, I am not really sure why there is a poll manager at all..... maybe because of the periodic, and msg sinks which are never actually used?  Anyway, pollmgr + 'loop sinks' seem to be overkil
at the moment, so I will not use that approach in the C# client, and will opt for something a bit more direct for now.

- When Udp receives a message from the network it hands it off to the callbacks, which are located in the backends (p2p, etc.)  Because the backends can have
more than one connection, each are first checked via HandlesMessage, which does an address check:
```
  return _peer_addr.sin_addr.S_un.S_addr == from.sin_addr.S_un.S_addr &&
    _peer_addr.sin_port == from.sin_port;
```
to make sure that the endpoint (udpprotocol) that sent the message doesn't handle it.  This makes sense to me, but not sure if the C# model will end up this way...
Anyway, UdpProtocol uses an array of function pointers for the message specific handles, indexed by the message type...  I guess that it slightly better than a branch, so I will look into a similar implementation for the C# client....