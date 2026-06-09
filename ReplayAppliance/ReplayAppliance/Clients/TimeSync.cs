namespace funscrew;

// ========================================================================================================
public class TimeSync
{
  public const int FRAME_WINDOW_SIZE = 40;
  public const int MIN_UNIQUE_FRAMES = 10;
  public const int MIN_FRAME_ADVANTAGE = 3;
  public const int MAX_FRAME_ADVANTAGE = 9;

  // NOTE: These are treated like circular buffers....
  // REFACTOR: '_localAdvantage' / '_remoteAdvantage'
  protected int[] _local = new int[TimeSync.FRAME_WINDOW_SIZE];
  protected int[] _remote = new int[TimeSync.FRAME_WINDOW_SIZE];
  protected GameInput[] _last_inputs = new GameInput[MIN_UNIQUE_FRAMES];
  protected int _next_prediction;

  // -----------------------------------------------------------------------------------------------------
  public TimeSync()
  {
    _next_prediction = FRAME_WINDOW_SIZE * 3;
  }

  // -----------------------------------------------------------------------------------------------------
  public void rollback_frame(ref GameInput input, int localAdvantage, int remoteAdvantage)
  {
    // Remember the last frame and frame advantage
    _last_inputs[input.frame % MIN_UNIQUE_FRAMES] = input;
    _local[input.frame % TimeSync.FRAME_WINDOW_SIZE] = localAdvantage;
    _remote[input.frame % TimeSync.FRAME_WINDOW_SIZE] = remoteAdvantage;
  }

  // -----------------------------------------------------------------------------------------------------
  static int count = 0;
  public int recommend_frame_wait_duration(bool require_idle_input)
  {
    // Average our local and remote frame advantages
    int i, sum = 0;
    float advantage, radvantage;
    for (i = 0; i < TimeSync.FRAME_WINDOW_SIZE; i++)
    {
      sum += _local[i];
    }
    advantage = sum / (float)TimeSync.FRAME_WINDOW_SIZE;

    sum = 0;
    for (i = 0; i < TimeSync.FRAME_WINDOW_SIZE; i++)
    {
      sum += _remote[i];
    }
    radvantage = sum / (float)TimeSync.FRAME_WINDOW_SIZE;

    count++;

    // See if someone should take action.  The person furthest ahead
    // needs to slow down so the other user can catch up.
    // Only do this if both clients agree on who's ahead!!
    if (advantage >= radvantage)
    {
      return 0;
    }

    // Both clients agree that we're the one ahead.  Split
    // the difference between the two to figure out how long to
    // sleep for.
    int sleep_frames = (int)(((radvantage - advantage) / 2) + 0.5);

    Utils.LogIt(LogCategories.TIMESYNC, "iteration %d:  sleep frames is %d", count, sleep_frames);

    // Some things just aren't worth correcting for.  Make sure
    // the difference is relevant before proceeding.
    if (sleep_frames < MIN_FRAME_ADVANTAGE)
    {
      return 0;
    }

    // Make sure our input had been "idle enough" before recommending
    // a sleep.  This tries to make the emulator sleep while the
    // user's input isn't sweeping in arcs (e.g. fireball motions in
    // Street Fighter), which could cause the player to miss moves.
    if (require_idle_input)
    {
      for (i = 1; i < MIN_UNIQUE_FRAMES; i++)
      {
        if (!_last_inputs[i].equal(ref _last_inputs[0]))
        {
          Utils.LogIt(LogCategories.TIMESYNC, "iteration %d:  rejecting due to input stuff at position %d...!!!", count, i);
          return 0;
        }
      }
    }

    // Success!!! Recommend the number of frames to sleep and adjust
    return Math.Min(sleep_frames, MAX_FRAME_ADVANTAGE);
  }

}


