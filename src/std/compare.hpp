#pragma once

namespace kstd {
	template<typename A, typename B = A>
	constexpr int threeway(const A& a, const B& b) requires requires(A a, B b) {
		a == b;
		a < b;
		a > b;
	} {
		if (a < b) {
			return -1;
		}
		else if (a > b) {
			return 1;
		}
		else {
			return 0;
		}
	}
}
