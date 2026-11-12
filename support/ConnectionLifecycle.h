// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <atomic>

// The network thread publishes Drained only after its last callback/cleanup.
// UI cleanup acknowledges Idle. A second connection cannot overlap either.
class ConnectionLifecycle {
public:
	bool TryStart() {
		State idle = State::Idle;
		return fState.compare_exchange_strong(idle, State::Running);
	}
	void RequestStop() {
		State running = State::Running;
		fState.compare_exchange_strong(running, State::Stopping);
	}
	bool StopRequested() const { return fState == State::Stopping; }
	bool NetworkActive() const { return fState == State::Running || fState == State::Stopping; }
	bool IsIdle() const { return fState == State::Idle; }
	bool IsDrained() const { return fState == State::Drained; }
	void MarkDrained() { fState = State::Drained; }
	void AcknowledgeCleanup() {
		State drained = State::Drained;
		fState.compare_exchange_strong(drained, State::Idle);
	}
private:
	enum class State { Idle, Running, Stopping, Drained };
	std::atomic<State> fState{State::Idle};
};
