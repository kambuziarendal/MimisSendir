// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <atomic>

namespace Upload {
// A one-shot linearization point before handing an OOB message to gloox. This
// never waits for a socket: UI cancellation/closure must not acquire a lock
// held by send(). Cancellation cannot retract an already committed message.
class SendCommitGate {
public:
	bool Cancel() {
		State pending = State::Pending;
		return fState.compare_exchange_strong(pending, State::Cancelled);
	}
	bool TryCommit() {
		State pending = State::Pending;
		return fState.compare_exchange_strong(pending, State::Committed);
	}
private:
	enum class State { Pending, Cancelled, Committed };
	std::atomic<State> fState{State::Pending};
};
}
