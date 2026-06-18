using System.Diagnostics;

namespace funscrew.Clients
{

  // ==============================================================================================================================
  /// <summary>
  /// This is an example client that can be used to replay inputs from the remote peer at a given time interval.
  /// </summary>
  internal class InputEchoClient : GGPOClient
  {

    // Button state tracking....
    public const int BUTTON_COUNT = 12;
    public const int BUTTON_LEFT = 4;
    public const int BUTTON_RIGHT = 5;
    private bool[] Buttons = new bool[BUTTON_COUNT];

    // NOTE: This needs to be more of a ring-buffer.....
    private InputEcho[] Echoes = null!;
    private int EchoWriteIndex = -1;
    private int EchoReadIndex = 0;

    private int MaxEchoFrames = 0;

    private InputEchoOptions EchoOptions = null!;
    private Stopwatch Clock = Stopwatch.StartNew();

    // --------------------------------------------------------------------------------------------------------------------------
    public InputEchoClient(GGPOClientOptions options_, InputEchoOptions echoOps_, IUdpBlaster udp_, IClockSource clock_) 
      : base(options_, udp_, clock_)
    {
      EchoOptions = echoOps_;

      // This will size the echo frames appropriately.
      MaxEchoFrames = (int)(EchoOptions.DelayFrameCount * 2);

      Echoes = new InputEcho[MaxEchoFrames];
      for (int i = 0; i < Echoes.Length; i++)
      {
        Echoes[i] = new InputEcho();
      }
    }

    // --------------------------------------------------------------------------------------------------------------------------
    // We aren't going to send anything unless we have an echo frame ready....
    protected override unsafe bool AddLocalInput(byte[] values, int isize)
    {
      int time = (int)Clock.ElapsedMilliseconds;

      var nextEcho = Echoes[EchoReadIndex];

      if (nextEcho.EchoFrame != InputEcho.AVAILABLE && this._sync.GetFrameCount() > nextEcho.EchoFrame)
      {
        {
          byte[] replayBuffer = new byte[values.Length];
          for (int i = 0; i < BUTTON_COUNT; i++)
          {
            Utils.SetBit(replayBuffer, nextEcho.Buttons[i], i);

          }
          for (int i = 0; i < isize; i++)
          {
            values[i] = replayBuffer[i];
          }

          nextEcho.Clear();

          // Increment the read index for next time....
          EchoReadIndex = (EchoReadIndex + 1) % MaxEchoFrames;
        }
      }

      return base.AddLocalInput(values, isize);
    }


    // --------------------------------------------------------------------------------------------------------------------------
    protected unsafe override void OnUdpProtocolPeerEvent(ref UdpEvent evt, GGPOEndpoint endpoint)
    {
      base.OnUdpProtocolPeerEvent(ref evt, endpoint);
      if (evt.type == EEventType.Input)
      {


        // Loop back on itself.  Only add the data if the next items is 'available'
        int nextIndex = (EchoWriteIndex + 1) % MaxEchoFrames;
        if (Echoes[nextIndex].EchoFrame == InputEcho.AVAILABLE)
        {
          // TODO: Only do replays for buttons that we care about.  If there are no button inputs, for example, then we don't want to
          // do a replay....
          // That would be the first 12 bits, BTW.
          bool anyPressed = false;
          fixed (byte* pData = evt.u.input.data)
          {

            for (int i = 0; i < Buttons.Length; i++)
            {
              Buttons[i] = Utils.ReadBit(pData, i);
              anyPressed |= Buttons[i];
            }
          }

          if (anyPressed)
          {
            EchoWriteIndex = nextIndex;

            // We can invert directions here....
            if (EchoOptions.InvertLeftRightControls)
            {
              bool l = Buttons[BUTTON_LEFT];
              bool r = Buttons[BUTTON_RIGHT];
              if (l) { Buttons[BUTTON_RIGHT] = true; Buttons[BUTTON_LEFT] = false; }
              if (r) { Buttons[BUTTON_LEFT] = true; Buttons[BUTTON_RIGHT] = false; }
            }

            Array.Copy(Buttons, Echoes[EchoWriteIndex].Buttons, BUTTON_COUNT);
            Echoes[EchoWriteIndex].EchoFrame = _sync.GetFrameCount() + EchoOptions.DelayFrameCount;
            Echoes[EchoWriteIndex].PlayerIndex = endpoint.PlayerIndex; // playerIndex;
          }
        }

      }
    }

  }



  // ==============================================================================================================================
  class InputEcho
  {
    public const int AVAILABLE = -1;

    public int EchoFrame = AVAILABLE;
    //public int ReplayTime = AVAILABLE;     // -1 indicates this slot is available.....
    public ushort PlayerIndex;
    public bool[] Buttons = new bool[InputEchoClient.BUTTON_COUNT];         // The buttons that we want to replay.

    // --------------------------------------------------------------------------------------------------------------------------
    internal void Clear()
    {
      EchoFrame = AVAILABLE;
      PlayerIndex = ushort.MaxValue;
      Array.Clear(Buttons);
    }
  }

}
