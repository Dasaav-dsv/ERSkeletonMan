#pragma once

#include <immintrin.h>

// https://meghprkh.github.io/blog/posts/c++-force-inline/
// Local always inline macro.
// Makes the compiler less likely to turn what should be a compile time calculation into a runtime function call.
// (mostly applies to MSVC)
#if defined(__clang__)
#define POINTERCHAIN_FORCE_INLINE [[gnu::always_inline]] [[gnu::gnu_inline]] extern inline

#elif defined(__GNUC__)
#define POINTERCHAIN_FORCE_INLINE [[gnu::always_inline]] inline

#elif defined(_MSC_VER)
#pragma warning(error: 4714)
#define FASTSTRING_FORCE_INLINE __forceinline

#else
#error Unsupported compiler
#endif

template <size_t N, typename T> FASTSTRING_FORCE_INLINE bool strcmp_fast(char* mem, const T(&str)[N])
{
	constexpr size_t size = N * sizeof(T);

	if constexpr (size == 0) return true;
	else if constexpr (size == 1) return *mem == *reinterpret_cast<const char*>(str);
	else if constexpr (size == 2) return *reinterpret_cast<const short*>(mem) == *reinterpret_cast<const short*>(str);
	else if constexpr (size <= 4) return !((*reinterpret_cast<const int*>(mem) ^ *reinterpret_cast<const int*>(str)) << (4 - size));
	else if constexpr (size <= 8) return !((*reinterpret_cast<const long long*>(mem) ^ *reinterpret_cast<const long long*>(str)) << (8 - size));
	else if constexpr (size <= 16) {
		__m128i str1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(mem));
		__m128i str2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(str));
		__m128i result = _mm_xor_si128(str1, str2);
		if constexpr (size != 16) result = _mm_slli_si128(result, 16 - size);
		return _mm_testz_si128(result, result);
	}
	else return strcmp_fast(mem, *reinterpret_cast<const T(*)[16 / sizeof(T)]>(reinterpret_cast<const T*>(str)))
		&& strcmp_fast(mem + 16, *reinterpret_cast<const T(*)[N - 16 / sizeof(T)]>(reinterpret_cast<const T*>(str) + 16 / sizeof(T)));
}

#ifdef FASTSTRING_FORCE_INLINE
#undef FASTSTRING_FORCE_INLINE
#endif