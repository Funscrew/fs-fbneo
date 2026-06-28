using drewCo.Tools;
using funscrew;
using funscrew.Clients;
using System.Net.Mime;

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
    /// This test case shows:
    /// 1. If one or more players aren't able to connect / don't connect in time, the replay appliance will timeout / fail.
    /// 2. After it times out / disconnects, the players will then be able to sync their game as normal.
    /// NOTE: This only addresses the case of the timeout happenening from the perspective of the replay appliance.  It does not
    /// consider the case where the players' aren't able to connect / communicate at all (for whatever reason).
    /// </summary>
    [Test]
    public unsafe void PlayersCanStillSyncIfReplayApplianceDisconnects()
    {
      const string P1_NAME = "Joe";
      const string P2_NAME = "Archie";
      const string GAME_NAME = "test-game-1";
      const string GAME_VERSION = "123";

      TestContext context = CreateTestContext(GAME_NAME, GAME_VERSION, P1_NAME, P2_NAME);

      (var frontDoor, var replayAppliance) = context.CreateReplayAppliance();
      ReplaySession rpSess = frontDoor.BeginSession(context.SessionId, context.SessionOptions);

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
      Assert.That(rpSess.EndpointCount, Is.EqualTo(1), "There should be one connected client!");


      // P1 / P2 should still be syncing at this point.
      Assert.That(p1Remote._current_state, Is.EqualTo(EClientState.Syncing), "P1 should still be syncing!");
      Assert.That(p2Remote._current_state, Is.EqualTo(EClientState.Syncing), "P2 should still be syncing!");

      // If both players are syncing, then no exchange of inputs should happen.
      Assert.That(p1Remote.TotalInputsSent, Is.EqualTo(0), "No inputs should have been sent at this time!");
      Assert.That(p2Remote.TotalInputsSent, Is.EqualTo(0), "No inputs should have been sent at this time!");

      Assert.That(replayAppliance.ActiveSessionCount, Is.EqualTo(1), "There should be one active sessions now!");

      // After a while, if the second player doesn't connect / sync, then the replay appliance should
      // send a disconnect signal, and abandon the session.
      var testTime = GGPOClientOptions.DEFAULT_CONNECT_TIMEOUT * 2;
      var evt = context.RunUtilEvent(p1, EEventCode.GGPO_EVENTCODE_DISCONNECTED_FROM_PEER, testTime);
      Assert.IsTrue(evt.isReplayEndpoint == 1, "The disconnecting endpoint should be from the replay appliance.");
      Assert.NotNull(evt);

      // Let's make sure that the replay appliance is cleaned up correctly as well.
      Assert.That(replayAppliance.ActiveSessionCount, Is.EqualTo(0), "There should be no active sessions now!");
      Assert.That(replayAppliance.SessionsStarted, Is.EqualTo(1));
      Assert.That(replayAppliance.SessionsEnded, Is.EqualTo(1));


      // Now after some time, the two players should sync up, and the replay appliance should be marked as complete / disconnected.
      context.RunGame(500);

      Assert.That(p1Remote._current_state, Is.EqualTo(EClientState.Running), "p1 remote should be running now");
      Assert.That(p2Remote._current_state, Is.EqualTo(EClientState.Running), "p2 remote should be running now");
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

      (var frontDoor, var replayAppliance) = context.CreateReplayAppliance();
      ReplaySession rpSess = frontDoor.BeginSession(context.SessionId, context.SessionOptions);

      // Each of the players will need to send their data to the replay appliance.
      var p1 = context.Player1Client;
      var raep1 = p1.AddReplayAppliance(REPLAY_APPLIANCE_HOST, REPLAY_APPLIANCE_PORT, REPLAY_APPLIANCE_TIMEOUT);

      var p2 = context.Player2Client;
      var raep2 = p2.AddReplayAppliance(REPLAY_APPLIANCE_HOST, REPLAY_APPLIANCE_PORT, REPLAY_APPLIANCE_TIMEOUT);

      // NOTE: Choose as little time as possible to get the clients synced.
      // Maybe some kind of a callback or 'run until'...?
      // TODO: Implement a 'run until....' here to minimize connection time?
      const int STARTUP_TIME = 2000;
      context.RunGame(STARTUP_TIME);
      Assert.That(replayAppliance.Errors.Count, Is.EqualTo(0), "There should be no listed errors!");

      // Show that the endpoints on the session are running:
      Assert.That(rpSess.EndpointCount, Is.EqualTo(2), "There should be two connected clients!");
      Assert.That(rpSess.Endpoints[0]._current_state, Is.EqualTo(EClientState.Running));
      Assert.That(rpSess.Endpoints[1]._current_state, Is.EqualTo(EClientState.Running));

      // Show that both players are also synced with the replay appliance (on those endpoints)
      Assert.That(raep1._current_state, Is.EqualTo(EClientState.Running), "Replay appliance endpoint for p1 should be listed as running!");
      Assert.That(raep2._current_state, Is.EqualTo(EClientState.Running), "Replay appliance endpoint for p2 should be listed as running!");

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
      p1GGPO.ID = 1;

      var p2GGPO = CreateGGPOClient(ops2, ops1, testQueue, sessId);
      p2GGPO.ID = 2;

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
