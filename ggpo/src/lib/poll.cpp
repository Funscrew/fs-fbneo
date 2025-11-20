/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#include "types.h"
#include "poll.h"

PollManager::PollManager(void) :
	_handle_count(0),
	_start_time(0)
{
	/*
	 * Create a dummy handle to simplify things.
	 */
	_handles[_handle_count++] = CreateEvent(NULL, true, false, NULL);
}

void PollManager::RegisterHandle(IPollSink* sink, HANDLE h, void* cookie)
{
	ASSERT(_handle_count < MAX_POLLABLE_HANDLES - 1);

	_handles[_handle_count] = h;
	_handle_sinks[_handle_count] = PollSinkCallback(sink, cookie);
	_handle_count++;
}

// ------------------------------------------------------------------------------------------------------------------
// NOTE: Not used...
void PollManager::RegisterMsgSink(IPollSink* sink, void* cookie)
{
	_msg_sinks.push_back(PollSinkCallback(sink, cookie));
}

// ------------------------------------------------------------------------------------------------------------------
// NOTE: Not used...
void PollManager::RegisterPeriodicSink(IPollSink* sink, int interval, void* cookie)
{
	_periodic_sinks.push_back(PollPeriodicSinkCallback(sink, cookie, interval));
}

// ------------------------------------------------------------------------------------------------------------------
void PollManager::RegisterLoopSink(IPollSink* sink, void* cookie)
{
	_loop_sinks.push_back(PollSinkCallback(sink, cookie));
}


// ------------------------------------------------------------------------------------------------------------------
// OBSOLETE: Running the pump like this will just make everything super slow....
// Not really sure what the purpose of this might be....
void PollManager::Run()
{
	while (Pump(100)) {
		continue;
	}
}

// ------------------------------------------------------------------------------------------------------------------
// REFACTOR: Return value is never checked.
// REFACTOR: WaitForMultipleObjects can be removed.
// REFACTOR: Periodic and message sinks can be removed.
bool PollManager::Pump(int timeout)
{
	bool finished = false;

  // REFACTOR: Wait time code can be dropped as there are no periodic sinks!
	if (_start_time == 0) {
		_start_time = Platform::GetCurrentTimeMS();
	}
	int elapsed = Platform::GetCurrentTimeMS() - _start_time;
	int maxwait = ComputeWaitTime(elapsed);
	if (maxwait != INFINITE) {
		timeout = MIN(timeout, maxwait);
	}

	// NOTE: I am 99% sure that all of the handle based code and waiting for them is
	// not used...  In fact it always appears to just timeout.....
	int i = 0;
	int res = WaitForMultipleObjects(_handle_count, _handles, false, timeout);

	// NOTE: I added this just to demonstrate what is going on.....
	bool isTimeout = res == WAIT_TIMEOUT;
  if (!isTimeout)  {
    i = i;
  }
	if (res >= WAIT_OBJECT_0 && res < WAIT_OBJECT_0 + _handle_count) {
		i = res - WAIT_OBJECT_0;
		finished = !_handle_sinks[i].sink->OnHandlePoll(_handle_sinks[i].cookie) || finished;
	}

	// NOTE: Not sure why a backend would want to have a periodic sink...  Maybe aggregate stats
	// or update logging?
	for (i = 0; i < _periodic_sinks.size(); i++) {
		PollPeriodicSinkCallback& cb = _periodic_sinks[i];
		if (cb.interval + cb.last_fired <= elapsed) {
			cb.last_fired = (elapsed / cb.interval) * cb.interval;
			finished = !cb.sink->OnPeriodicPoll(cb.cookie, cb.last_fired) || finished;
		}
	}

	// NOTE: It appears that message sinks, and loop sinks are basically the same thing.
	// I guess that they are broken up like this to keep the concepts distinct.
	// Like a message sink happens for certain messages, and the loop sink happens each 'frame'.
	for (i = 0; i < _msg_sinks.size(); i++) {
		PollSinkCallback& cb = _msg_sinks[i];
		finished = !cb.sink->OnMsgPoll(cb.cookie) || finished;
	}

	for (i = 0; i < _loop_sinks.size(); i++) {
		PollSinkCallback& cb = _loop_sinks[i];
		finished = !cb.sink->OnLoopPoll(cb.cookie) || finished;
	}
	return finished;
}

int
PollManager::ComputeWaitTime(int elapsed)
{
	int waitTime = INFINITE;
	size_t count = _periodic_sinks.size();

	if (count > 0) {
		for (int i = 0; i < count; i++) {
			PollPeriodicSinkCallback& cb = _periodic_sinks[i];
			int timeout = (cb.interval + cb.last_fired) - elapsed;
			if (waitTime == INFINITE || (timeout < waitTime)) {
				waitTime = MAX(timeout, 0);
			}
		}
	}
	return waitTime;
}
