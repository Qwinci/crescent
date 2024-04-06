#pragma once
#include "type_traits.hpp"

namespace kstd {
	template<typename T>
	concept integral = is_integral_v<T>;

	namespace __detail {
		template<typename T, typename U>
		concept same_helper = kstd::is_same_v<T, U>;
	}

	template<typename T, typename U>
	concept same_as = __detail::same_helper<T, U> && __detail::same_helper<U, T>;
}
