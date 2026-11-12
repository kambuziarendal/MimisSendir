#include "support/LifetimeRegistry.h"
#include <cstdlib>
#include <iostream>
static int checks;
static void Check(bool ok) { ++checks; if (!ok) std::abort(); }
int main()
{
	LifetimeRegistry registry;
	int sameAddress = 0, other = 0;
	Check(!registry.Matches(&sameAddress, 0));
	registry.Remember(&sameAddress);
	const auto first = registry.Token(&sameAddress);
	Check(first != 0 && registry.Matches(&sameAddress, first));
	registry.Forget(&sameAddress);
	Check(!registry.Matches(&sameAddress, first));
	registry.Remember(&sameAddress); // Reused allocation address, old message still queued.
	const auto second = registry.Token(&sameAddress);
	Check(second != first);
	Check(!registry.Matches(&sameAddress, first));
	Check(registry.Matches(&sameAddress, second));
	registry.Remember(&other);
	Check(!registry.Matches(&other, second));
	registry.Clear(); // A drained connection invalidates every pending UI address.
	Check(!registry.Matches(&sameAddress, second));
	registry.Remember(&sameAddress);
	Check(registry.Token(&sameAddress) != second);
	Check(!registry.Matches(&sameAddress, second));
	std::cout << checks << " queued UI lifetime checks passed\n";
}
