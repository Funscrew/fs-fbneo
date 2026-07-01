using drewCo.Tools.Logging;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using static System.Net.Mime.MediaTypeNames;

namespace funscrew.Clients
{

  // ==============================================================================================================================
  /// <summary>
  /// This is very much like GGPOClient, but it is only for sending replay data to an appliance.
  /// </summary>
  public class ReplayEndpoint : GGPOEndpoint
  {
    public static int MAX_ACKS = 0x100;

    private ReplaySession Session = null!;

    /// <summary>
    /// The acks that we still need to send out.
    /// </summary>
    private RingBuffer<GameInput> _PendingAcks = null!;

    private int StartTime = 0;


    // --------------------------------------------------------------------------------------------------------------------------  
    public ReplayEndpoint(ReplaySession session_, GGPOEndpointOptions ops_, ConnectStatus[] localConnectStatus_)
      : base(session_, ops_, localConnectStatus_)
    {
      this.Session = session_;
      _PendingAcks = new RingBuffer<GameInput>(MAX_ACKS);

      StartTime = Client.CurTime;
      EndpointType = EEndpointType.ReplayAppliance;
    }

    // --------------------------------------------------------------------------------------------------------------------------  
    public override void OnLoopPoll()
    {
      if (_current_state == EClientState.Disconnected) { return; }

      if (_current_state == EClientState.Syncing)
      {
        int elapsed = Client.CurTime - StartTime;
        if (elapsed >= Client.ConnectTimeout)
        {
          // We are going to disconnect.
          Disconnect(Client.CurrentFrame, true);
          ConnectionTimedOut = true;
          return;
        }
      }

      base.OnLoopPoll();
      SendPendingAcks();
    }

    // --------------------------------------------------------------------------------------------------------------------------  
    protected override bool OnInput(ref UdpMsg msg, int msgLen)
    {
      // The replay client needs to do that same thing as the normal endpoint by keeping
      // a set of ACKS to send back to the client. 
      // In this case our parent is going to be a replay appliance so we may need to keep a separate list
      // of stuff that needs to be acked / resolved on our end.....
      bool res = base.OnInput(ref msg, msgLen);

      // Housekeeping.  We can get rid of all confirmed acks.
      // TODO: I'd like to log the size of these ring buffers to see what is typical.  Is there really a certain amount of 'overdraw' in the system always?
      while (_PendingAcks.Size > 0 && _PendingAcks.Front().frame < msg.u.input.ack_frame)
      {
        Utils.LogIt(LogCategories.INPUT, "ACK: Throwing away pending ACK frame %d", _PendingAcks.Front().frame);
        _last_acked_input = _PendingAcks.Front();
        _PendingAcks.Pop();

        if (this.Session != null)
        {
          this.Session.MergeInput(ref _last_acked_input, this.PlayerIndex);
        }
      }

      return res;
    }

    // --------------------------------------------------------------------------------------------------------------------------  
    protected override void SendInputEvent(ref GameInput input)
    {
      base.SendInputEvent(ref input);

      // Send the ACK for this input!
      SendInputAck(ref input);
    }

    // --------------------------------------------------------------------------------------------------------------------------  
    private void SendInputAck(ref GameInput input)
    {
      if (_PendingAcks.IsFull)
      {
        Log.Error($"ACK BUFFER full for: player: {this.PlayerIndex}.  Disconnecting!");
        Disconnect(0);

        // We aren't going to fail, we are simply going to disconnect the client!

        throw new InvalidOperationException($"{nameof(_PendingAcks)} buffer is full!  System will fail!");
      }
      _PendingAcks.Push(input);

      SendPendingAcks();
    }

    // --------------------------------------------------------------------------------------------------------------------------  
    /// <summary>
    /// This is like 'SendPendingOutput' but it is for input ACK messages.
    /// </summary>
    private void SendPendingAcks()
    {
      // GameInput last;
      // NEW:
      // We will collect all of the pending acks and send a message for each:
      // In the future we can combine them all into a single message to ACK mulitple inputs.
      if (_PendingAcks.Size > 0)
      {
        var last = _last_acked_input;
        var front = _PendingAcks.Front();
        Utils.ASSERT(last.frame == -1 || last.frame + 1 == front.frame);

        var msg = new UdpMsg(EMsgType.InputAck);
        msg.u.input_ack.start_frame = _PendingAcks[0].frame;

        UInt16 useCount = 1;
        int expectedFrame = _PendingAcks[0].frame;
        for (int i = 1; i < _PendingAcks.Size; i++)
        {
          // NOTE: These checks are really more exploratory than anything....
          // I am pretty sure that neither case will be encountered as part of the OOP handling...
          if (expectedFrame == _PendingAcks[i].frame)
          {
            // A duplicate frame...
            Log.Debug("duplicate frame in ACK encountered!");
          }

          ++expectedFrame;
          if (expectedFrame != _PendingAcks[i].frame)
          {
            Log.Debug("incorrect next frame....");
            break;
          }

          ++useCount;
        }

        msg.u.input_ack.frame_count = useCount;
        SendMsg(ref msg);
      }

    }

  }
}
