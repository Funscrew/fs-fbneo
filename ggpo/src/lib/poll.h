/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#ifndef _POLL_H
#define _POLL_H

#include "static_buffer.h"

#define MAX_POLLABLE_HANDLES     64


class IPollSink {
public:
	virtual ~IPollSink() {}
	virtual bool OnHandlePoll(void*) { return true; }
	virtual bool OnMsgPoll(void*) { return true; }
	virtual bool OnPeriodicPoll(void*, int) { return true; }
	virtual bool OnLoopPoll(void*) { return true; }
};

// ==========================================================================================================
class PollManager {
public:
	PollManager(void);
	void RegisterHandle(IPollSink* sink, HANDLE h, void* cookie = NULL);
	void RegisterMsgSink(IPollSink* sink, void* cookie = NULL);
	void RegisterPeriodicSink(IPollSink* sink, int interval, void* cookie = NULL);
	void RegisterLoopSink(IPollSink* sink, void* cookie = NULL);

	void Run();
	bool Pump(int timeout);

protected:
	int ComputeWaitTime(int elapsed);

	struct PollSinkCallback {
		IPollSink* sink;
		void* cookie;
		PollSinkCallback() : sink(NULL), cookie(NULL) {}
		PollSinkCallback(IPollSink* sink_, void* cookie_) : sink(sink_), cookie(cookie_) {}
	};

	struct PollPeriodicSinkCallback : public PollSinkCallback {
		int         interval;
		int         last_fired;
		PollPeriodicSinkCallback() : PollSinkCallback(NULL, NULL), interval(0), last_fired(0) {}
		PollPeriodicSinkCallback(IPollSink* s, void* c, int i) :
			PollSinkCallback(s, c), interval(i), last_fired(0) {
		}
	};

	int               _start_time;
	int               _handle_count;
	HANDLE            _handles[MAX_POLLABLE_HANDLES];
	PollSinkCallback        _handle_sinks[MAX_POLLABLE_HANDLES];

	StaticBuffer<PollSinkCallback, 16>          _msg_sinks;
	StaticBuffer<PollPeriodicSinkCallback, 16>  _periodic_sinks;

	StaticBuffer<PollSinkCallback, 16>          _loop_sinks;
};

#endif
