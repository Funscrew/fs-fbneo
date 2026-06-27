using drewCo.Tools;
using funscrew;
using funscrew.Clients;
using System.Net.Mime;
using System.Net.WebSockets;

namespace funscrewTesters
{

  // ==============================================================================================================================
  public class NetworkTesters : TestBase
  {
    // --------------------------------------------------------------------------------------------------------------------------
    public NetworkTesters()
    {
      FileTools.EmptyDirectory("replays");
      FileTools.CreateDirectory("replays");
    }

    // --------------------------------------------------------------------------------------------------------------------------
    /// <summary>
    /// This test case shows that while it may be able to connect to a replay appliance at some point,
    /// it is possible for the connection to go bad / get dropped.
    /// When this condition is detected (during merge) we will want to send out the disconnect signal + log
    /// the failure in the replay data.
    /// </summary>
    [Test]
    public unsafe void ReplayApplianceWontSyncUntilAllPlayersAreConnected()
    {
      const string P1_NAME = "Joe";
      const string P2_NAME = "Archie";
      const string GAME_NAME = "test-game";
      const string GAME_VERSION = "123";

      TestContext context = CreateTestContext(GAME_NAME, GAME_VERSION, P1_NAME, P2_NAME);

      //var ops = new GGPOClientOptions("test-game", 0, REPLAY_APPLIANCE_PORT, Defaults.PROTOCOL_VERSION, context.SessionId);
      //ops.Callbacks = CreateDefaultCallbacks();

      //var cbh = new CallbackHandler();
      //ops.Callbacks.on_event = cbh.OnEvent;


      ReplayAppliance replayAppliance = CreateTestReplayAppliance(context);
      var spOps = new SessionPrimerOptions()
      {
        Port = FRONT_DOOR_PORT,
      };
      var frontDoor = new SimSessionPrimer(spOps, replayAppliance);
      ReplaySession rpSess = frontDoor.BeginSession(context.SessionId, context.SessionOptions);

      context.SetSessionPrimer(frontDoor);
      context.SetReplayAppliance(replayAppliance);

      // We will connect only one player at this time, and run the system for a bit.
      GGPOClient p1 = context.Player1Client;
      p1.AddReplayAppliance(REPLAY_APPLIANCE_HOST, REPLAY_APPLIANCE_PORT, REPLAY_APPLIANCE_TIMEOUT);

      var p1Remote = p1.GetRemotePlayer() as SimGGPOEndpoint;
      Assert.IsNotNull(p1Remote);

      GGPOClient p2 = context.Player2Client;
      var p2Remote = p2.GetRemotePlayer() as SimGGPOEndpoint;
      Assert.IsNotNull(p2Remote);

      const int STARTUP_TIME = 100;
      context.RunGame(STARTUP_TIME);

      Assert.That(replayAppliance.Errors.Count, Is.EqualTo(0), "There should be no listed errors!");
      Assert.That(rpSess.ClientCount, Is.EqualTo(1), "There should be one connected client!");


      // P1 / P2 should still be syncing at this point.
      Assert.That(p1Remote._current_state, Is.EqualTo(EClientState.Syncing), "P1 should still be syncing!");
       Assert.That(p2Remote._current_state, Is.EqualTo(EClientState.Syncing), "P2 should still be syncing!");

      // If both players are syncing, then no exchange of inputs should happen.
      Assert.That(p1Remote.TotalInputsSent, Is.EqualTo(0), "No inputs should have been sent at this time!");
      Assert.That(p2Remote.TotalInputsSent, Is.EqualTo(0), "No inputs should have been sent at this time!");


      // After a while, if the second player doesn't connect / sync, then the replay appliance should
      // send a disconnect signal, and abandon the session.
      context.RunUtilEvent(p1, EEventCode.GGPO_EVENTCODE_DISCONNECTED_FROM_PEER, 5000);


      throw new InvalidOperationException("Create some kind of 'run until event' function!");

      const int SIM_TIME = 2000;      // More than enough time for the mismatch to be detected / dropped.
      context.RunGame(SIM_TIME);


      //// NOTE: The conditions below are all wrong.
      //// If only one of the clients have connected, there should not be any exchange of inputs
      //// until BOTH clients have connected + synced.

      //// We want to get a count for the number of times that we have sent inputs to the remote.
      //// We should not have any until everything is all synced up....
      //Assert.That(p1Remote.TotalInputsSent, Is.GreaterThan(0), "Inputs should be exchanged at this point.");

      //// NOTE: Is there any way that we can get disconnect events?

      //const int SIM_TIME = 2000;      // More than enough time for the mismatch to be detected / dropped.
      //context.RunGame(SIM_TIME);


      //// Show that there are no longer any connections....
      //Assert.IsTrue(rpSess.IsComplete, "The session should be in the complete state.");
      //Assert.That(rpSess.ClientCount, Is.EqualTo(0), "There should no longer be any connected clients!");

      //var recorder = rpSess.Recorder;
      //Assert.IsTrue(recorder.RecordingComplete, "The recording should be marked as complete!");
      //Assert.IsTrue(recorder.HasError, "The recorder should be marked as having an error!");

    }

    // --------------------------------------------------------------------------------------------------------------------------
    /// <summary>
    /// Shows that two clients can connect to the same replay client, and that client is able to record
    /// their inputs.
    /// NOTE: This test in particular doesn't do anything about handling dropped or OO packets.
    /// --> edge case, an input that is sent to player 1 could be dropped, but the replay client could receive it.  How do we handle that case?
    /// </summary>
    [Test]
    public unsafe void CanUseReplayAppliance()
    {
      const string P1_NAME = "Joe";
      const string P2_NAME = "Archie";
      const string GAME_NAME = "test-game";
      const string GAME_VERSION = "123";

      TestContext context = CreateTestContext(GAME_NAME, GAME_VERSION, P1_NAME, P2_NAME);

      var ops = new GGPOClientOptions("test-game", 0, REPLAY_APPLIANCE_PORT, Defaults.PROTOCOL_VERSION, context.SessionId);
      ops.Callbacks = CreateDefaultCallbacks();

      var cbh = new CallbackHandler();
      ops.Callbacks.on_event = cbh.OnEvent;

      ReplayAppliance replayAppliance = CreateTestReplayAppliance(context);
      var spOps = new SessionPrimerOptions()
      {
        Port = FRONT_DOOR_PORT,
      };
      var frontDoor = new SimSessionPrimer(spOps, replayAppliance);

      ReplaySession rpSess = frontDoor.BeginSession(context.SessionId, context.SessionOptions);

      context.SetSessionPrimer(frontDoor);
      context.SetReplayAppliance(replayAppliance);

      // Each of the players will need to send their data to the replay appliance.
      var p1 = context.Player1Client;
      p1.AddReplayAppliance(REPLAY_APPLIANCE_HOST, REPLAY_APPLIANCE_PORT, REPLAY_APPLIANCE_TIMEOUT);

      var p2 = context.Player2Client;
      p2.AddReplayAppliance(REPLAY_APPLIANCE_HOST, REPLAY_APPLIANCE_PORT, REPLAY_APPLIANCE_TIMEOUT);


      // NOTE: Choose as little time as possible to get the clients synced.
      // Maybe some kind of a callback or 'run until'...?
      const int STARTUP_TIME = 50;
      context.RunGame(STARTUP_TIME);
      Assert.That(replayAppliance.Errors.Count, Is.EqualTo(0), "There should be no listed errors!");

      // At this point we should have two connected clients on the replay appliance.
      Assert.That(rpSess.ClientCount, Is.EqualTo(2), "There should be two connected clients!");

      // Make sure that the players are synced up as well as the GGPO client itself.
      var remote1 = context.Player1Client.GetRemotePlayer();
      Assert.That(context.Player1Client._synchronizing, Is.False);
      Assert.That(remote1._current_state, Is.EqualTo(EClientState.Running));

      var remote2 = context.Player2Client.GetRemotePlayer();
      Assert.That(context.Player2Client._synchronizing, Is.False);
      Assert.That(remote2._current_state, Is.EqualTo(EClientState.Running));

      // Confirm that both of the endpoints are syned.
      var rc1 = rpSess.GetEndpoint(0);
      Assert.NotNull(rc1);

      var rc2 = rpSess.GetEndpoint(1);
      Assert.NotNull(rc2);

      Assert.That(rc1._current_state, Is.EqualTo(EClientState.Running), "Client 1 should be synced!");
      Assert.That(rc2._current_state, Is.EqualTo(EClientState.Running), "Client 2 should be synced!");

      // Now that both clients are running, they should be exchanging input.
      // We want to inject a known set of inputs for each to test our recording capability.

      int[] curInput = new int[2];
      const int ONE_SECOND = 1000;
      context.RunGame(ONE_SECOND, (data, playerindex, curTime) =>
      {
        curInput[playerindex] += 1;
        int useVal = curInput[0];

        data[0] = (byte)(useVal & 0xFF);
        data[1] = (byte)(useVal >> 8 & 0xFF);
        data[2] = (byte)(useVal >> 16 & 0xFF);
        data[3] = (byte)(useVal >> 24 & 0xFF);
      });

      Assert.Inconclusive("Show that we have recorded a certain number of inputs!");

      // TODO: Way more to do!
      // Inputs need to be exchanged between the players + we need to confirm that we are receiving and merging them correctly!
      throw new Exception("Please complete this test!");

    }

    // --------------------------------------------------------------------------------------------------------------------------
    /// <summary>
    /// This shows that we are able to create two clients and have them communicate over a virtual, ideal network.
    /// The main purpose of this is to show that we can indeed simulate a network, which will allow us to
    /// develop the protocol bits faster + make automated tests to hadle all kinds of scenarios.
    /// </summary>
    [Test]
    public unsafe void CanSimluateNetworkGame()
    {
      const string P1_NAME = "Joe";
      const string P2_NAME = "Archie";
      const string GAME_NAME = "test-game";
      const string GAME_VERSION = "123";

      TestContext context = CreateTestContext(GAME_NAME, GAME_VERSION, P1_NAME, P2_NAME);

      var p1 = context.Player1;
      var p2 = context.Player2;

      var p1GGPO = context.Player1Client;
      var p2GGPO = context.Player2Client;

      const int MAX_TIME = 50;
      context.RunGame(MAX_TIME);

      // Here we can check to see if the players are synced or not...
      Assert.That(p1._current_state == EClientState.Running, "P1 should be listed as running!");
      Assert.That(p2._current_state == EClientState.Running, "P2 should be listed as running!");

      // TODO: consider this logic.  The player names should be exchanged on handshake...
      var p1l = p1GGPO.GetLocalPlayer();
      var p2l = p2GGPO.GetLocalPlayer();

      Assert.That(p2.GetPlayerName(), Is.EqualTo(P2_NAME));
      Assert.That(p1.GetPlayerName(), Is.EqualTo(P1_NAME));
    }




    // --------------------------------------------------------------------------------------------------------------------------
    /// <summary>
    /// A convenient way to setup a test context, with two clients, replay appliance, etc.
    /// </summary>
    protected TestContext CreateTestContext(string gameName, string gameVersion, string p1Name, string p2Name)
    {
      ulong sessId = GetNextSessionId();

      // This is how we actually move the messages around....
      var timeSource = new SimClock();
      var testQueue = new TestMessageQueue();

      var ops1 = new TestPlayerOptions()
      {
        PlayerIndex = PLAYER1_INDEX,
        Host = PLAYER1_HOST,
        Port = PLAYER1_PORT,
        TimeSource = timeSource,
        InputBuffer = new byte[5 * MAX_PLAYERS],
        PlayerName = p1Name
      };

      var ops2 = new TestPlayerOptions()
      {
        PlayerIndex = PLAYER2_INDEX,
        Host = PLAYER2_HOST,
        Port = PLAYER2_PORT,
        TimeSource = timeSource,
        InputBuffer = new byte[5 * MAX_PLAYERS],
        PlayerName = p2Name
      };

      var p1GGPO = CreateGGPOClient(ops1, ops2, testQueue, sessId);
      var p2GGPO = CreateGGPOClient(ops2, ops1, testQueue, sessId);

      // NOTE: If we use 'GetLocalPlayer' then the test fails.  This is part of some weird implementation
      // detail of how the GGPOEndpoints/Client code runs.  I am pretty sure this is by design, and I have
      // no intention of attempting to 'fix' it.
      //var p2 = p1GGPO.GetRemotePlayer();
      //var p1 = p2GGPO.GetRemotePlayer();

      var context = new TestContext(sessId, timeSource, testQueue, new[] { p1GGPO, p2GGPO }, new[] { ops1.InputBuffer, ops2.InputBuffer });

      var sessOps = new SessionOptions()
      {
        Clock = context.Clock,
        GameName = gameName,
        GameVersion = gameVersion,
        TotalInputSize = 10,
        MaxPlayerCount = 2,
        PlayerNames = new[] { p1Name, p2Name }
      };
      context.SessionOptions = sessOps;

      return context;
    }
  }

}


// ==============================================================================================================================
public class CallbackHandler
{
  // --------------------------------------------------------------------------------------------------------------------------
  public unsafe bool OnEvent(ref GGPOEvent e)
  {
    return true;
  }
}
