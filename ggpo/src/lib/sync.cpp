/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#include "sync.h"

Sync::Sync(UdpMsg::connect_status *connect_status) :
 _local_connect_status(connect_status),
 _input_queues(NULL)
{
   _curFrame = 0;
   _last_confirmed_frame = -1;
   _max_prediction_frames = 0;
   memset(&_savedstate, 0, sizeof(_savedstate));
}

Sync::~Sync()
{
   /*
    * Delete frames manually here rather than in a destructor of the SavedFrame
    * structure so we can efficently copy frames via weak references.
    */
   for (int i = 0; i < ARRAY_SIZE(_savedstate.frames); i++) {
      _callbacks.free_buffer(_savedstate.frames[i].buf);
   }
   delete [] _input_queues;
   _input_queues = NULL;
}

void
Sync::Init(Sync::Config &config)
{
   _config = config;
   _callbacks = config.callbacks;
   _curFrame = 0;
   _rollingback = false;

   _max_prediction_frames = config.num_prediction_frames;

   CreateQueues(config);
}

void
Sync::SetLastConfirmedFrame(int frame) 
{   
   _last_confirmed_frame = frame;
   if (_last_confirmed_frame > 0) {
      for (int i = 0; i < _config.num_players; i++) {
         _input_queues[i].DiscardConfirmedFrames(frame - 1);
      }
   }
}

// ------------------------------------------------------------------------------------------------------------------------
bool Sync::AddLocalInput(PlayerID playerIndex, GameInput &input)
{
   int frames_behind = _curFrame - _last_confirmed_frame; 
   if (_curFrame >= _max_prediction_frames && frames_behind >= _max_prediction_frames) {
      Log("Rejecting input from emulator: reached prediction barrier.\n");
      return false;
   }

   if (_curFrame == 0) {
      SaveCurrentFrame();
   }

   Log("Sending undelayed local frame %d to queue %d.\n", _curFrame, playerIndex);
   input.frame = _curFrame;
   _input_queues[playerIndex].AddInput(input);

   return true;
}

// ------------------------------------------------------------------------------------------------------------------------
void Sync::AddRemoteInput(PlayerID playerIndex, GameInput &input)
{
   _input_queues[playerIndex].AddInput(input);
}

// ------------------------------------------------------------------------------------------------------------------------
int Sync::GetConfirmedInputs(void *values, int size, int frame)
{
   int disconnect_flags = 0;
   char *output = (char *)values;

   ASSERT(size >= _config.num_players * _config.input_size);

   memset(output, 0, size);
   for (int i = 0; i < _config.num_players; i++) {
      GameInput input;
      if (_local_connect_status[i].disconnected && frame > _local_connect_status[i].last_frame) {
         disconnect_flags |= (1 << i);
         input.erase();
      } else {
         _input_queues[i].GetConfirmedInput(frame, &input);
      }
      memcpy(output + (i * _config.input_size), input.bits, _config.input_size);
   }
   return disconnect_flags;
}

int Sync::SynchronizeInputs(void *values, int totalSize)
{
   int disconnect_flags = 0;
   char* output = (char *)values;

   // Ensure a minimum amount of data so we don't overrun the buffer...
   // Shouldn't we expect that totalSize is always the same... ??
   ASSERT(totalSize >= _config.num_players * _config.input_size);

   memset(output, 0, totalSize);
   for (int i = 0; i < _config.num_players; i++) {
      GameInput input;
      if (_local_connect_status[i].disconnected && _curFrame > _local_connect_status[i].last_frame) {
         disconnect_flags |= (1 << i);
         input.erase();
      } else {
         _input_queues[i].GetInput(_curFrame, &input);
      }
      memcpy(output + (i * _config.input_size), input.bits, _config.input_size);
   }
   return disconnect_flags;
}

void
Sync::CheckSimulation(int timeout)
{
   int seek_to;
   if (!CheckSimulationConsistency(&seek_to)) {
      AdjustSimulation(seek_to);
   }
}

void
Sync::IncrementFrame(void)
{
   _curFrame++;
   SaveCurrentFrame();
}

void
Sync::AdjustSimulation(int seek_to)
{
   int prevFrame = _curFrame;
   int count = _curFrame - seek_to;   // This is assumed to be positive b/c we are rolling back to an earlier frame.  Therefore, _framecount is always > seek_to.

   Log("Catching up\n");
   _rollingback = true;

   /*
    * Flush our input queue and load the last frame.
    */
   LoadFrame(seek_to);
   ASSERT(_curFrame == seek_to);

   // Now that we have updated _framecount to seek_to, it will be == to (oldFrameCount - count).

   /*
    * Advance frame by frame (stuffing notifications back to 
    * the master).
    */
   ResetPrediction(_curFrame);
   for (int i = 0; i < count; i++) {
     // NOTE 2: This is where the _framecount would probably be updated to the correct version.
      _callbacks.rollback_frame(0);
   }

   // NOTE: This assert will fail if _framecount is not correctly incremented in the above for loop.  rollback_frame should increment it!
   ASSERT(_curFrame == prevFrame);

   _rollingback = false;

   Log("---\n");   
}

void
Sync::LoadFrame(int frame)
{
   // find the frame in question
   if (frame == _curFrame) {
      Log("Skipping NOP.\n");
      return;
   }

   // Move the head pointer back and load it up
   _savedstate.head = FindSavedFrameIndex(frame);
   SavedFrame *state = _savedstate.frames + _savedstate.head;

   Log("=== Loading frame info %d (size: %d  checksum: %08x).\n",
       state->frame, state->cbuf, state->checksum);

   ASSERT(state->buf && state->cbuf);
   _callbacks.load_game_state(state->buf, state->cbuf);

   // Reset framecount and the head of the state ring-buffer to point in
   // advance of the current frame (as if we had just finished executing it).
   _curFrame = state->frame;
   _savedstate.head = (_savedstate.head + 1) % ARRAY_SIZE(_savedstate.frames);
}

void
Sync::SaveCurrentFrame()
{
   /*
    * See StateCompress for the real save feature implemented by FinalBurn.
    * Write everything into the head, then advance the head pointer.
    */
   SavedFrame *state = _savedstate.frames + _savedstate.head;
   if (state->buf) {
      _callbacks.free_buffer(state->buf);
      state->buf = NULL;
   }
   state->frame = _curFrame;
   _callbacks.save_game_state(&state->buf, &state->cbuf, &state->checksum, state->frame);

   Log("=== Saved frame info %d (size: %d  checksum: %08x).\n", state->frame, state->cbuf, state->checksum);
   _savedstate.head = (_savedstate.head + 1) % ARRAY_SIZE(_savedstate.frames);
}

Sync::SavedFrame&
Sync::GetLastSavedFrame()
{
   int i = _savedstate.head - 1;
   if (i < 0) {
      i = ARRAY_SIZE(_savedstate.frames) - 1;
   }
   return _savedstate.frames[i];
}


int
Sync::FindSavedFrameIndex(int frame)
{
   int i, count = ARRAY_SIZE(_savedstate.frames);
   for (i = 0; i < count; i++) {
      if (_savedstate.frames[i].frame == frame) {
         break;
      }
   }
   if (i == count) {
      ASSERT(FALSE);
   }
   return i;
}


bool
Sync::CreateQueues(Config &config)
{
   delete [] _input_queues;
   _input_queues = new InputQueue[_config.num_players];

   for (int i = 0; i < _config.num_players; i++) {
      _input_queues[i].Init(i, _config.input_size);
   }
   return true;
}

bool
Sync::CheckSimulationConsistency(int *seekTo)
{
   int first_incorrect = GameInput::NullFrame;
   for (int i = 0; i < _config.num_players; i++) {
      int incorrect = _input_queues[i].GetFirstIncorrectFrame();
      Log("considering incorrect frame %d reported by queue %d.\n", incorrect, i);

      if (incorrect != GameInput::NullFrame && (first_incorrect == GameInput::NullFrame || incorrect < first_incorrect)) {
         first_incorrect = incorrect;
      }
   }

   if (first_incorrect == GameInput::NullFrame) {
      Log("prediction ok.  proceeding.\n");
      return true;
   }
   *seekTo = first_incorrect;
   return false;
}

void
Sync::SetFrameDelay(int queue, int delay)
{
   _input_queues[queue].SetFrameDelay(delay);
}


void
Sync::ResetPrediction(int frameNumber)
{
   for (int i = 0; i < _config.num_players; i++) {
      _input_queues[i].ResetPrediction(frameNumber);
   }
}


bool
Sync::GetEvent(Event &e)
{
   if (_event_queue.size()) {
      e = _event_queue.front();
      _event_queue.pop();
      return true;
   }
   return false;
}


