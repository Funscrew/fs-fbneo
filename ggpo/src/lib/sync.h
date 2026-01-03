/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#ifndef _SYNC_H
#define _SYNC_H

#include "types.h"
#include "ggponet.h"
#include "game_input.h"
#include "input_queue.h"
#include "ring_buffer.h"
#include "network/udp_msg.h"

// REFACTOR: Make this a const + move to consts container
#define MAX_PREDICTION_FRAMES    8

class SyncTestBackend;

class Sync {
public:
  // REFACTOR: SyncOptions
  struct Config {
    GGPOSessionCallbacks    callbacks;
    int                     num_prediction_frames;
    int                     num_players;
    int                     input_size;
  };

  // REFACTOR: SyncEvent
  struct Event {
    // I think that this can also be simplifed.  Seems like everything is just 'confirmed input'
    enum {
      ConfirmedInput,
    } type;
    // NOTE: This union is not necessary.
    union {
      struct {
        GameInput   input;
      } confirmedInput;
    } u;
  };

public:
  Sync(UdpMsg::connect_status* connect_status);
  virtual ~Sync();

  void Init(Config& config);

  void SetLastConfirmedFrame(int frame);
  void SetFrameDelay(int queue, int delay);
  bool AddLocalInput(uint8_t playerIndex, GameInput& input);
  void AddRemoteInput(uint8_t playerIndex, GameInput& input);

  // NOTE: This function appears to be unused!
  int GetConfirmedInputs(void* values, int size, int frame);
  int SynchronizeInputs(void* values, int totalSize);

  void CheckSimulation(int timeout);
  void AdjustSimulation(int seek_to);
  void IncrementFrame(void);

  int GetFrameCount() { return _curFrame; }
  bool InRollback() { return _rollingback; }

  bool GetEvent(Event& e);

protected:
  friend SyncTestBackend;

  struct SavedFrame {
    byte* buf;
    int      cbuf;
    int      frame;
    int      checksum;
    SavedFrame() : buf(NULL), cbuf(0), frame(-1), checksum(0) {}
  };
  struct SavedState {
    SavedFrame frames[MAX_PREDICTION_FRAMES + 2];
    int head;     // Index of the saved frame data.
  };

  void LoadFrame(int frame);
  void SaveCurrentFrame();
  int FindSavedFrameIndex(int frame);
  SavedFrame& GetLastSavedFrame();

  bool CreateQueues(Config& config);
  bool CheckSimulationConsistency(int* seekTo);
  void ResetPrediction(int frameNumber);

protected:
  GGPOSessionCallbacks _callbacks;
  SavedState     _savedstate;
  Config         _config;             // REFACTOR: rename to 'Options'

  bool           _rollingback;
  int            _last_confirmed_frame;
  int            _curFrame;                         // Number of the current frame.  This can be adjusted during rollbacks.
  int            _max_prediction_frames;

  InputQueue* _input_queues;

  RingBuffer<Event, 32> _event_queue;
  UdpMsg::connect_status* _local_connect_status;
};

#endif

