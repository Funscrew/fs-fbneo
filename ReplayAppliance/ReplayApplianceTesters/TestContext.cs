using funscrew;
using funscrew.Clients;

namespace funscrewTesters
{
  // ==============================================================================================================================
  public class TestContext
  {
    const int TIME_INTERVAL = 1;
    const int FRAME_INTERVAL = 16;

    public SimClock Clock { get; private set; }
    public TestMessageQueue MsgQueue { get; private set; }
    private List<GGPOClient> AllClients = new List<GGPOClient>();
    private List<byte[]> InputBuffers = new List<byte[]>();
    public ReplayAppliance? ReplayAppliance { get; private set; } = null;
    public SessionPrimer? SessionPrimer { get; private set; } = null;

    public int LastFrame {get; private set; } = -1;

    public ulong SessionId { get; private set; }

    // --------------------------------------------------------------------------------------------------------------------------
    public TestContext(ulong sessionId_, SimClock timeSource_, TestMessageQueue msgQueue_, IList<GGPOClient> allClients_, IList<byte[]> inputBuffers_, ReplayAppliance? replay_ = null)
    {
      SessionId = sessionId_;
      Clock = timeSource_;
      MsgQueue = msgQueue_;
      AllClients.AddRange(allClients_);
      InputBuffers.AddRange(inputBuffers_);
      ReplayAppliance = replay_;
    }

    public GGPOClient Player1Client { get { return AllClients[0]; } }
    public GGPOClient Player2Client { get { return AllClients[1]; } }

    public GGPOEndpoint Player1 { get { return AllClients[1].GetRemotePlayer(); } }
    public GGPOEndpoint Player2 { get { return AllClients[0].GetRemotePlayer(); } }

    // --------------------------------------------------------------------------------------------------------------------------
    // TEMP: We will use a contructor based version later....  maybe....
    public void SetSessionPrimer(SessionPrimer primer_)
    {
      this.SessionPrimer = primer_;
    }

    // --------------------------------------------------------------------------------------------------------------------------
    // TEMP: We will use a contructor based version later....  maybe....
    public void SetReplayAppliance(ReplayAppliance replay_)
    {
      this.ReplayAppliance = replay_;
    }

    // --------------------------------------------------------------------------------------------------------------------------
    /// <param name="setInputs">Callback for each frame and player so their inputs for the frame can be set.  Args are: input data, player index, currentTime</param>
    public void RunGame(int totalTime, Action<byte[], int, int>? setInputs = null)
    {

      // The total number of 'frames' that we want to simulate in this case.
      //  const int MAX_FRAMES = 50;
      for (int curTime = 0; curTime < totalTime; curTime++)
      {
        Clock.AddTime(TIME_INTERVAL);

        //// NOTE: We only want to send the heartbeat every so often......
        //// And the purpose of the heartbeat is so that blocking calls in production netcode
        //// have something to do every so often if there are no incoming packets...
        //if (SessionPrimer != null) {
        //  // Send the heartbeat signal...
        //  (SessionPrimer as SimSessionPrimer)?.SendHeartbeat();
        //// SessionPrimer.
        //}

        if (ReplayAppliance != null)
        {
          ReplayAppliance.Update(); //DoPoll(0);
        }

        if (curTime % FRAME_INTERVAL == 0)
        {
          // TODO: I want to change the inputs per frame.  Data doesn't matter, just that it can be exchanged.
          // Probably just increment the bits....
          // p1Input[0] = (byte)(i & 0xFF);
          int len = AllClients.Count;
          for (int clientIndex = 0; clientIndex < len; clientIndex++)
          {
            var c = AllClients[clientIndex];

            if (setInputs != null)
            {
              setInputs(InputBuffers[clientIndex], c.GetLocalPlayer().PlayerIndex, curTime);
            }

            Program.RunFrame(c, InputBuffers[clientIndex]);

            ++LastFrame;
          }

        }
        else
        {
          // TODO: A proper idle() function.....    (see Program.cs for example)
          // This is where we would send out the player inputs and so on....
          int len = AllClients.Count;
          for (int j = 0; j < len; j++)
          {
            var c = AllClients[j];
            c.Idle();
          }
        }
      }

    }
  }




}
