#include "support/ConnectionLifecycle.h"
#include <cstdlib>
#include <iostream>
#include <thread>
#include <atomic>
static int checks;
static void Check(bool ok) { ++checks; if (!ok) std::abort(); }
int main()
{
	ConnectionLifecycle state;
	Check(state.IsIdle());
	state.RequestStop(); Check(state.IsIdle());
	Check(state.TryStart()); Check(!state.TryStart());
	Check(state.NetworkActive());
	state.RequestStop(); Check(state.StopRequested());
	Check(!state.TryStart()); // Cancel during startup cannot be lost by another start.
	state.AcknowledgeCleanup(); Check(!state.IsIdle());
	state.MarkDrained(); Check(!state.NetworkActive());
	Check(!state.TryStart()); // UI/session cleanup must also complete first.
	state.AcknowledgeCleanup(); Check(state.IsIdle());
	Check(state.TryStart());
	state.MarkDrained(); state.AcknowledgeCleanup(); Check(state.IsIdle());
	for (int i = 0; i < 100; ++i) {
		ConnectionLifecycle gate;
		std::atomic_bool begin{false}; bool a = false, b = false;
		std::thread first([&] { while (!begin) std::this_thread::yield(); a = gate.TryStart(); });
		std::thread second([&] { while (!begin) std::this_thread::yield(); b = gate.TryStart(); });
		begin = true; first.join(); second.join();
		Check(a != b);
		gate.RequestStop(); Check(!gate.TryStart());
		gate.MarkDrained(); Check(!gate.TryStart());
		gate.AcknowledgeCleanup(); Check(gate.TryStart());
	}
	std::cout << checks << " connection lifecycle gate checks passed\n";
}
