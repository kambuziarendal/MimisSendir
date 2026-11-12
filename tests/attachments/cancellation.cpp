#include "support/UploadCancellation.h"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>
static int checks;
static void Check(bool ok) { ++checks; if (!ok) std::abort(); }
int main()
{
	Upload::SendCommitGate cancelled;
	Check(cancelled.Cancel());
	Check(!cancelled.TryCommit());
	Check(!cancelled.Cancel());
	Upload::SendCommitGate committed;
	Check(committed.TryCommit());
	Check(!committed.Cancel());
	Check(!committed.TryCommit());
	// Both operation orders are covered above. Race their actual implementation
	// and verify exactly one wins, without depending on scheduler ordering.
	for (int i = 0; i < 100; ++i) {
		Upload::SendCommitGate gate;
		std::atomic_bool start{false};
		bool send = false, cancel = false;
		std::thread sender([&] { while (!start.load()) std::this_thread::yield(); send = gate.TryCommit(); });
		std::thread closer([&] { while (!start.load()) std::this_thread::yield(); cancel = gate.Cancel(); });
		start = true;
		sender.join(); closer.join();
		Check(send != cancel);
		Check(!gate.TryCommit());
	}
	std::cout << checks << " upload cancellation gate checks passed\n";
}
