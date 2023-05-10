#pragma once

#include <stdint.h>
#include <float.h>
#include <cmath>
#include <algorithm>
#include <immintrin.h>

#define is16Aligned(mem) ( reinterpret_cast<uintptr_t>(mem) & 15 == 0 )

#define UMTX { { { 1.0f, 0.0f, 0.0f, 0.0f },\
				 { 0.0f, 1.0f, 0.0f, 0.0f },\
				 { 0.0f, 0.0f, 1.0f, 0.0f },\
				 { 0.0f, 0.0f, 0.0f, 1.0f } } }

struct alignas(16) ViewMatrix { float mtx[4][4]; };

struct m128Matrix { __m128 r[4]; };

template <typename T> inline constexpr T sign(const T val)
{
	return static_cast<T>((static_cast<T>(0) < val)) - static_cast<T>((val < static_cast<T>(0)));
}

class V4D {
public:
	__m128 v4{};

	enum CoordinateAxis : int
	{
		X = 0,
		Z = 1,
		Y = 2,
		W = 3
	};

	V4D() {}

	V4D(const V4D& v)
	{
		this->v4 = v.v4;
	}

	V4D(__m128 v)
	{
		this->v4 = v;
	}

	V4D(__m128i v)
	{
		this->v4 = _mm_castsi128_ps(v);
	}

	V4D(const float v[4])
	{
		this->v4 = v != nullptr ? _mm_loadu_ps(v) : _mm_setzero_ps();
	}

	V4D(const float f0, const float f1, const float f2, const float f3)
	{
		this->v4 = _mm_set_ps(f3, f2, f1, f0);
	}

	V4D(const float f0, const float f1, const float f2)
	{
		__m128 v = _mm_set_ps(f2, f2, f1, f0);
		this->v4 = _mm_insert_ps(v, _mm_setzero_ps(), 0b00110000);
	}

	V4D(const float f)
	{
		this->v4 = _mm_set1_ps(f);
	}

	V4D(const V4D axis, const float angle)
	{
		const float halfa = angle * 0.5f;

		__m128 v = _mm_mul_ps(axis, _mm_set1_ps(sinf(halfa)));
		this->v4 = _mm_insert_ps(v, _mm_set_ss(cosf(halfa)), 0b00110000);
	}

	V4D(const V4D v1, const V4D v2)
	{
		V4D v = v1.cross(v2);
		v[3] = v1 * v2 + sqrtf(v1.length2() * v1.length2());
		*this = v.normalize();
	}

	~V4D() {}

	operator __m128() const
	{
		return this->v4;
	}

	operator __m128i() const
	{
		return _mm_castps_si128(this->v4);
	}

	float& operator [] (const int index)
	{
		return reinterpret_cast<float*>(&this->v4)[index];
	}

	float operator [] (const int index) const
	{
		return reinterpret_cast<const float*>(&this->v4)[index];
	}

	volatile float& operator [] (const int index) volatile
	{
		return reinterpret_cast<volatile float*>(&this->v4)[index];
	}

	V4D& operator = (const V4D v)
	{
		this->v4 = v;
		return *this;
	}

	V4D& operator = (const float v[4])
	{
		this->v4 = _mm_loadu_ps(v);
		return *this;
	}

	V4D operator + (const V4D v) const
	{
		return _mm_add_ps(*this, v);
	}

	void operator += (const V4D v)
	{
		this->v4 = _mm_add_ps(*this, v);
	}

	V4D operator - (const V4D v) const
	{
		return _mm_sub_ps(*this, v);
	}

	void operator -= (const V4D v)
	{
		this->v4 = _mm_sub_ps(*this, v);
	}

	V4D operator * (const float s) const
	{
		return _mm_mul_ps(*this, _mm_set1_ps(s));
	}

	void operator *= (const float s)
	{
		this->v4 = _mm_mul_ps(*this, _mm_set1_ps(s));
	}

	V4D operator / (const float s) const
	{
		return _mm_div_ps(*this, _mm_set1_ps(s));
	}

	void operator /= (const float s)
	{
		this->v4 = _mm_div_ps(*this, _mm_set1_ps(s));
	}

	bool isfinite() const
	{
		return _mm_movemask_ps(_mm_cmpunord_ps(*this, *this)) == 0;
	}

	bool iszero() const
	{
		return _mm_testz_si128(*this, *this);
	}

	__m128 hadd() const // https://stackoverflow.com/questions/6996764/fastest-way-to-do-horizontal-sse-vector-sum-or-other-reduction#:~:text=for%20the%20optimizer.-,SSE3,-float%20hsum_ps_sse3(__m128
	{
		__m128 sumsh = _mm_movehdup_ps(*this);
		__m128 shsum = _mm_add_ps(*this, sumsh);
		sumsh = _mm_movehl_ps(sumsh, shsum);
		shsum = _mm_add_ss(shsum, sumsh);

		return shsum;
	}

	float length() const
	{
		if (!this->isfinite()) return 0.0f;

		V4D v = _mm_mul_ps(*this, *this);

		return _mm_cvtss_f32(_mm_sqrt_ss(v.hadd()));
	}

	float length2() const
	{
		if (!this->isfinite()) return 0.0f;

		V4D v = _mm_mul_ps(*this, *this);

		return _mm_cvtss_f32(v.hadd());
	}

	float inRange(const V4D v, const float range) const
	{
		V4D r = v - *this;
		r = _mm_mul_ps(r, r);
		r = r.hadd();

		if (_mm_comigt_ss(r, _mm_setzero_ps())) {
			if (_mm_comile_ss(_mm_mul_ps(r, _mm_rsqrt_ss(r)), _mm_set_ss(range))) {
				return _mm_cvtss_f32(_mm_sqrt_ss(r));
			}
			else {
				return 0.0f;
			}
		}
		else {
			return range;
		}
	}

	V4D normalize() const
	{
		if (this->iszero() || !this->isfinite()) {
			return _mm_setzero_ps();
		}
		else {
			return _mm_div_ps(this->v4, _mm_set1_ps(this->length()));
		}
	}

	V4D scaleTo(const float s) const
	{
		return _mm_mul_ps(this->normalize(), _mm_set1_ps(s));
	}

	template <CoordinateAxis A = Z, bool Normalize = false> V4D flatten() const
	{
		V4D v = _mm_insert_ps(*this, _mm_setzero_ps(), A << 4);

		if constexpr (Normalize) {
			return v.normalize();
		}
		else {
			return v;
		}
	}

	float operator * (const V4D v) const
	{
		return _mm_cvtss_f32(V4D(_mm_mul_ps(*this, v)).hadd());
	}

	float dot3(const V4D v) const
	{
		return _mm_cvtss_f32(V4D(_mm_mul_ps(this->flatten<W>(), v.flatten<W>())).hadd());
	}

	V4D cross(const V4D v) const // https://geometrian.com/programming/tutorials/cross-product/index.php
	{
		__m128 tmp0 = _mm_shuffle_ps(*this, *this, _MM_SHUFFLE(3, 0, 2, 1));
		__m128 tmp1 = _mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 1, 0, 2));
		__m128 tmp2 = _mm_mul_ps(tmp0, v);
		__m128 tmp3 = _mm_mul_ps(tmp0, tmp1);
		__m128 tmp4 = _mm_shuffle_ps(tmp2, tmp2, _MM_SHUFFLE(3, 0, 2, 1));

		return _mm_sub_ps(tmp3, tmp4);
	}

	V4D projectOnto(const V4D v) const
	{
		return _mm_mul_ps(v, _mm_set1_ps(v * *this));
	}

	float sign2v(const V4D v) const
	{
		const float k = -1.0f;

		if (!_mm_testz_si128(_mm_castps_si128(_mm_mul_ps(*this, v)), _mm_set1_epi32(0x80000000))) {
			return k;
		}
		else {
			return k * k;
		}
	}

	V4D qConjugate() const
	{
		return _mm_xor_ps(*this, _mm_set_ps(0.0f, -0.0f, -0.0f, -0.0f));
	}

	V4D qTransform(const V4D v) const
	{
		const float v0 = v[0];
		const float v1 = v[1];
		const float v2 = v[2];
		const float v3 = v[3];

		const float v_0 = (*this)[0];
		const float v_1 = (*this)[1];
		const float v_2 = (*this)[2];

		const float v3v3_ = v3 * v3 - 0.5f;

		const float v0v1 = v[0] * v[1];
		const float v0v2 = v[0] * v[2];
		const float v0v3 = v[0] * v[3];

		const float v1v2 = v[1] * v[2];
		const float v1v3 = v[1] * v[3];

		const float v2v3 = v[2] * v[3];

		return V4D(2.0f * (v_0 * (v0 * v0 + v3v3_) + v_1 * (v0v1 - v2v3) + v_2 * (v0v2 + v1v3)),
			2.0f * (v_0 * (v0v1 + v2v3) + v_1 * (v1 * v1 + v3v3_) + v_2 * (v1v2 - v0v3)),
			2.0f * (v_0 * (v0v2 - v1v3) + v_1 * (v1v2 + v0v3) + v_2 * (v2 * v2 + v3v3_)));
	}

	V4D qMul(const V4D v) const
	{
		const float v0 = (*this)[0];
		const float v1 = (*this)[1];
		const float v2 = (*this)[2];
		const float v3 = (*this)[3];

		const float v_0 = v[0];
		const float v_1 = v[1];
		const float v_2 = v[2];
		const float v_3 = v[3];

		return V4D(v3 * v_0 + v0 * v_3 + v1 * v_2 - v2 * v_1,
			v3 * v_1 - v0 * v_2 + v1 * v_3 + v2 * v_0,
			v3 * v_2 + v0 * v_1 - v1 * v_0 + v2 * v_3,
			v3 * v_3 - v0 * v_0 - v1 * v_1 - v2 * v_2);
	}

	V4D qDiv(const V4D v) const
	{
		const float v0 = (*this)[0];
		const float v1 = (*this)[1];
		const float v2 = (*this)[2];
		const float v3 = (*this)[3];

		const float v_0 = v[0];
		const float v_1 = v[1];
		const float v_2 = v[2];
		const float v_3 = v[3];

		return V4D(v_3 * v0 - v_0 * v3 + v_1 * v2 - v_2 * v1,
			v_3 * v1 - v_0 * v2 - v_1 * v3 + v_2 * v0,
			v_3 * v2 + v_0 * v1 - v_1 * v0 - v_2 * v3,
			v_3 * v3 + v_0 * v0 + v_1 * v1 + v_2 * v2);
	}

	V4D qPow(const float pow) const
	{
		const float logFV = this->dot3(*this);
		const float logRV = sqrtf(logFV);
		const float logFR = (*this)[3];

		const float logS = logRV > FLT_EPSILON ? atan2f(logRV, logFR) / logRV : 0.0f;
		V4D logQ = *this * logS;
		logQ[3] = logf(logFV + logFR * logFR) / 2.0f;
		logQ *= pow;

		const float expFV = logQ.dot3(logQ);
		const float expRV = sqrtf(expFV);
		const float expRR = expf(logQ[3]);

		const float expS = expRV > FLT_EPSILON ? expRR * sinf(expRV) / expRV : 0.0f;
		logQ *= expS;
		logQ[3] = expRR * cosf(expRV);

		return logQ;
	}

	V4D qSlerp(const V4D v, const float t) const // http://number-none.com/product/Understanding%20Slerp,%20Then%20Not%20Using%20It/
	{
		float dot = *this * v;

		if (dot > 0.9995f) {
			V4D v_ = *this + (v - *this) * t;
			return v_.normalize();
		}

		dot = std::clamp(dot, -1.0f, 1.0f);
		float theta_0 = acosf(dot);
		float theta = theta_0 * t;

		V4D v_ = v - *this * dot;

		return *this * cosf(theta) + v_.normalize() * sinf(theta);
	}

	static V4D vmtxToQ(ViewMatrix* vmtx) // https://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/#:~:text=z%20%3D%200.25f%20*%20s%3B%0A%20%20%20%20%7D%0A%20%20%7D%0A%7D-,Alternative%20Method,-Christian%20has%20suggested
	{
		auto mtx = vmtx->mtx;
		__m128 c1 = _mm_set1_ps(1.0f);
		__m128 c2 = _mm_set1_ps(mtx[0][0]);
		__m128 c3 = _mm_set1_ps(mtx[1][1]);
		__m128 c4 = _mm_set1_ps(mtx[2][2]);

		// Make sure the signed zeroes are not optimized away (looking at you MSVC)
		__m128 sign = _mm_castsi128_ps(_mm_set_epi32(0x0, 0x80000000, 0x80000000, 0x0));
		c2 = _mm_xor_ps(c2, sign);
		sign = _mm_shuffle_ps(sign, sign, _MM_SHUFFLE(3, 2, 0, 1));
		c4 = _mm_xor_ps(c4, sign);
		sign = _mm_shuffle_ps(sign, sign, _MM_SHUFFLE(3, 1, 2, 0));
		c3 = _mm_xor_ps(c3, sign);

		__m128 result = _mm_add_ps(c1, c2);
		result = _mm_add_ps(result, c3);
		result = _mm_add_ps(result, c4);
		result = _mm_max_ps(result, _mm_setzero_ps());
		result = _mm_sqrt_ps(result);
		result = _mm_mul_ps(result, _mm_set1_ps(0.5f));

		sign = _mm_set_ps(0.0f, mtx[0][2], mtx[1][0], mtx[2][1]);
		sign = _mm_sub_ps(sign, _mm_set_ps(0.0f, mtx[2][0], mtx[0][1], mtx[1][2]));
		sign = _mm_and_ps(sign, _mm_castsi128_ps(_mm_set1_epi32(0x80000000)));

		result = _mm_or_ps(result, sign);

		return result;
	}
};