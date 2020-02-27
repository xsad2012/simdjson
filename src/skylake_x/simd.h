#ifndef SIMDJSON_SKYLAKE_X_SIMD_H
#define SIMDJSON_SKYLAKE_X_SIMD_H

#include "simdjson/portability.h"

#ifdef IS_X86_64

#include "simdjson/common_defs.h"
#include "skylake_x/intrinsics.h"

TARGET_SKYLAKE_X
namespace simdjson::skylake_x::simd {

  // Forward-declared so they can be used by splat and friends.
  template<typename Child>
  struct base {
    __m512i value;

    // Zero constructor
    really_inline base() : value{__m512i()} {}

    // Conversion from SIMD register
    really_inline base(const __m512i _value) : value(_value) {}

    // Conversion to SIMD register
    really_inline operator const __m512i&() const { return this->value; }
    really_inline operator __m512i&() { return this->value; }

    // Bit operations
    really_inline Child operator|(const Child other) const { return _mm512_or_si512(*this, other); }
    really_inline Child operator&(const Child other) const { return _mm512_and_si512(*this, other); }
    really_inline Child operator^(const Child other) const { return _mm512_xor_si512(*this, other); }
    really_inline Child bit_andnot(const Child other) const { return _mm512_andnot_si512(other, *this); }
    really_inline Child operator~() const { return *this ^ 0xFFu; }
    really_inline Child& operator|=(const Child other) { auto this_cast = (Child*)this; *this_cast = *this_cast | other; return *this_cast; }
    really_inline Child& operator&=(const Child other) { auto this_cast = (Child*)this; *this_cast = *this_cast & other; return *this_cast; }
    really_inline Child& operator^=(const Child other) { auto this_cast = (Child*)this; *this_cast = *this_cast ^ other; return *this_cast; }
  };

  // Forward-declared so they can be used by splat and friends.
  template<typename T>
  struct simd8;

  template<typename T, typename Mask=simd8<bool>>
  struct base8: base<simd8<T>> {
    typedef uint32_t bitmask_t;
    typedef uint64_t bitmask2_t;

    really_inline base8() : base<simd8<T>>() {}
    really_inline base8(const __m512i _value) : base<simd8<T>>(_value) {}

    really_inline Mask operator==(const simd8<T> other) const { return _mm512_cmpeq_epi8(*this, other); }

    template<int N=1>
    really_inline simd8<T> prev(const simd8<T> prev_chunk) const {
      return _mm512_alignr_epi8(*this, _mm512_permute2x128_si512(prev_chunk, *this, 0x21), 16 - N);
    }
  };

  // SIMD byte mask type (returned by things like eq and gt)
  template<>
  struct simd8<bool> {
    __mmask64 value;

    static really_inline simd8<bool> splat(bool _value) { return _value; }

    really_inline simd8<bool>() : base8() {}
    really_inline simd8<bool>(const __mmask64 _value) : value(_value) {}
    // really_inline simd8<bool>(const __m512i _value) : value(__mm512_movpi8_mask(_value)) {}
    // Splat constructor
    really_inline simd8<bool>(bool _value) : value(_value ? 0xFFFFFFFFFFFFFFFF : 0) {}

    really_inline simd8<bool> operator==(const simd8<bool> other) const { return value == other.value; }

    // Conversion to SIMD register
    // really_inline operator __m512i() const { return _mm512_movm_epi8(value); }

    template<int N=1>
    really_inline simd8<T> prev(const simd8<T> prev_chunk) const {
      // prev         | ABCD | EFGH | IJKL | MNOP |
      // current      | abcd | efgh | ijkl | mnop |
      // alignr_epi64 | MNOP | abcd | efgh | ijkl |
      // alignr_epi8  | Pabc | defg | hijk | lmno |
      return _mm512_alignr_epi8(_mm512_alignr_epi64(prev, current, 1), 16 - N);
    }

    really_inline __mmask64 to_bitmask() const { return value; }
    really_inline bool any() const { return value; }

    // Bit operations
    really_inline simd8<bool> operator|(const simd8<bool> other) const { return this->value | other.value; }
    really_inline simd8<bool> operator&(const simd8<bool> other) const { return this->value & other.value; }
    really_inline simd8<bool> operator^(const simd8<bool> other) const { return this->value ^ other.value; }
    really_inline simd8<bool> bit_andnot(const simd8<bool> other) const { return this->value & ~other.value; }
    really_inline simd8<bool> operator~() const { return ~this; }
    really_inline simd8<bool>& operator|=(const simd8<bool> other) { *this = *this | other; return *this; }
    really_inline simd8<bool>& operator&=(const simd8<bool> other) { *this = *this & other; return *this; }
    really_inline simd8<bool>& operator^=(const simd8<bool> other) { *this = *this ^ other; return *this; }
  };

  template<typename T>
  struct base8_numeric: base8<T> {
    static really_inline simd8<T> splat(T _value) { return _mm512_set1_epi8(_value); }
    static really_inline simd8<T> zero() { return _mm512_setzero_si512(); }
    static really_inline simd8<T> load(const T values[32]) {
      return _mm512_loadu_si512(reinterpret_cast<const __m512i *>(values));
    }
    // Repeat 16 values as many times as necessary (usually for lookup tables)
    static really_inline simd8<T> repeat_16(
      T v0,  T v1,  T v2,  T v3,  T v4,  T v5,  T v6,  T v7,
      T v8,  T v9,  T v10, T v11, T v12, T v13, T v14, T v15
    ) {
      return simd8<T>(
        v0, v1, v2, v3, v4, v5, v6, v7,
        v8, v9, v10,v11,v12,v13,v14,v15,
        v0, v1, v2, v3, v4, v5, v6, v7,
        v8, v9, v10,v11,v12,v13,v14,v15,
        v0, v1, v2, v3, v4, v5, v6, v7,
        v8, v9, v10,v11,v12,v13,v14,v15,
        v0, v1, v2, v3, v4, v5, v6, v7,
        v8, v9, v10,v11,v12,v13,v14,v15
      );
    }

    really_inline base8_numeric() : base8<T>() {}
    really_inline base8_numeric(const __m512i _value) : base8<T>(_value) {}

    // Store to array
    really_inline void store(T dst[32]) const { return _mm512_storeu_si512(reinterpret_cast<__m512i *>(dst), *this); }

    // Addition/subtraction are the same for signed and unsigned
    really_inline simd8<T> operator+(const simd8<T> other) const { return _mm512_add_epi8(*this, other); }
    really_inline simd8<T> operator-(const simd8<T> other) const { return _mm512_sub_epi8(*this, other); }
    really_inline simd8<T>& operator+=(const simd8<T> other) { *this = *this + other; return *(simd8<T>*)this; }
    really_inline simd8<T>& operator-=(const simd8<T> other) { *this = *this - other; return *(simd8<T>*)this; }

    // Perform a lookup assuming the value is between 0 and 16 (undefined behavior for out of range values)
    template<typename L>
    really_inline simd8<L> lookup_16(simd8<L> lookup_table) const {
      return _mm512_shuffle_epi8(lookup_table, *this);
    }
    template<typename L>
    really_inline simd8<L> lookup_16(
        L replace0,  L replace1,  L replace2,  L replace3,
        L replace4,  L replace5,  L replace6,  L replace7,
        L replace8,  L replace9,  L replace10, L replace11,
        L replace12, L replace13, L replace14, L replace15) const {
      return lookup_16(simd8<L>::repeat_16(
        replace0,  replace1,  replace2,  replace3,
        replace4,  replace5,  replace6,  replace7,
        replace8,  replace9,  replace10, replace11,
        replace12, replace13, replace14, replace15
      ));
    }
  };

  // Signed bytes
  template<>
  struct simd8<int8_t> : base8_numeric<int8_t> {
    really_inline simd8() : base8_numeric<int8_t>() {}
    really_inline simd8(const __m512i _value) : base8_numeric<int8_t>(_value) {}
    // Splat constructor
    really_inline simd8(int8_t _value) : simd8(splat(_value)) {}
    // Array constructor
    really_inline simd8(const int8_t values[32]) : simd8(load(values)) {}
    // Member-by-member initialization
    really_inline simd8(
      int8_t v0,  int8_t v1,  int8_t v2,  int8_t v3,  int8_t v4,  int8_t v5,  int8_t v6,  int8_t v7,
      int8_t v8,  int8_t v9,  int8_t v10, int8_t v11, int8_t v12, int8_t v13, int8_t v14, int8_t v15,
      int8_t v16, int8_t v17, int8_t v18, int8_t v19, int8_t v20, int8_t v21, int8_t v22, int8_t v23,
      int8_t v24, int8_t v25, int8_t v26, int8_t v27, int8_t v28, int8_t v29, int8_t v30, int8_t v31
      int8_t v32, int8_t v33, int8_t v34, int8_t v35, int8_t v36, int8_t v37, int8_t v38, int8_t v39,
      int8_t v40, int8_t v41, int8_t v42, int8_t v43, int8_t v44, int8_t v45, int8_t v46, int8_t v47
      int8_t v48, int8_t v49, int8_t v50, int8_t v51, int8_t v52, int8_t v53, int8_t v54, int8_t v55,
      int8_t v56, int8_t v57, int8_t v58, int8_t v59, int8_t v60, int8_t v61, int8_t v62, int8_t v63
    ) : simd8(_mm512_setr_epi8(
      v0,  v1,  v2,  v3,  v4,  v5,  v6,  v7,
      v8,  v9,  v10, v11, v12, v13, v14, v15,
      v16, v17, v18, v19, v20, v21, v22, v23,
      v24, v25, v26, v27, v28, v29, v30, v31
      v32, v33, v34, v35, v36, v37, v38, v39,
      v40, v41, v42, v43, v44, v45, v46, v47
      v48, v49, v50, v51, v52, v53, v54, v55,
      v56, v57, v58, v59, v60, v61, v62, v63
    )) {}
    // Repeat 16 values as many times as necessary (usually for lookup tables)
    really_inline static simd8<int8_t> repeat_16(
      int8_t v0,  int8_t v1,  int8_t v2,  int8_t v3,  int8_t v4,  int8_t v5,  int8_t v6,  int8_t v7,
      int8_t v8,  int8_t v9,  int8_t v10, int8_t v11, int8_t v12, int8_t v13, int8_t v14, int8_t v15
    ) {
      return simd8<int8_t>(
        v0, v1, v2, v3, v4, v5, v6, v7,
        v8, v9, v10,v11,v12,v13,v14,v15,
        v0, v1, v2, v3, v4, v5, v6, v7,
        v8, v9, v10,v11,v12,v13,v14,v15,
        v0, v1, v2, v3, v4, v5, v6, v7,
        v8, v9, v10,v11,v12,v13,v14,v15,
        v0, v1, v2, v3, v4, v5, v6, v7,
        v8, v9, v10,v11,v12,v13,v14,v15
      );
    }

    // Order-sensitive comparisons
    really_inline simd8<int8_t> max(const simd8<int8_t> other) const { return _mm512_max_epi8(*this, other); }
    really_inline simd8<int8_t> min(const simd8<int8_t> other) const { return _mm512_min_epi8(*this, other); }
    really_inline simd8<bool> operator>(const simd8<int8_t> other) const { return _mm512_cmpgt_epi8(*this, other); }
    really_inline simd8<bool> operator<(const simd8<int8_t> other) const { return _mm512_cmpgt_epi8(other, *this); }
  };

  // Unsigned bytes
  template<>
  struct simd8<uint8_t>: base8_numeric<uint8_t> {
    really_inline simd8() : base8_numeric<uint8_t>() {}
    really_inline simd8(const __m512i _value) : base8_numeric<uint8_t>(_value) {}
    // Splat constructor
    really_inline simd8(uint8_t _value) : simd8(splat(_value)) {}
    // Array constructor
    really_inline simd8(const uint8_t values[32]) : simd8(load(values)) {}
    // Member-by-member initialization
    really_inline simd8(
      uint8_t v0,  uint8_t v1,  uint8_t v2,  uint8_t v3,  uint8_t v4,  uint8_t v5,  uint8_t v6,  uint8_t v7,
      uint8_t v8,  uint8_t v9,  uint8_t v10, uint8_t v11, uint8_t v12, uint8_t v13, uint8_t v14, uint8_t v15,
      uint8_t v16, uint8_t v17, uint8_t v18, uint8_t v19, uint8_t v20, uint8_t v21, uint8_t v22, uint8_t v23,
      uint8_t v24, uint8_t v25, uint8_t v26, uint8_t v27, uint8_t v28, uint8_t v29, uint8_t v30, uint8_t v31
      uint8_t v32, uint8_t v33, uint8_t v34, uint8_t v35, uint8_t v36, uint8_t v37, uint8_t v38, uint8_t v39,
      uint8_t v40, uint8_t v41, uint8_t v42, uint8_t v43, uint8_t v44, uint8_t v45, uint8_t v46, uint8_t v47
      uint8_t v48, uint8_t v49, uint8_t v50, uint8_t v51, uint8_t v52, uint8_t v53, uint8_t v54, uint8_t v55,
      uint8_t v56, uint8_t v57, uint8_t v58, uint8_t v59, uint8_t v60, uint8_t v61, uint8_t v62, uint8_t v63
    ) : simd8(_mm512_setr_epi8(
      v0,  v1,  v2,  v3,  v4,  v5,  v6,  v7,
      v8,  v9,  v10, v11, v12, v13, v14, v15,
      v16, v17, v18, v19, v20, v21, v22, v23,
      v24, v25, v26, v27, v28, v29, v30, v31
      v32, v33, v34, v35, v36, v37, v38, v39,
      v40, v41, v42, v43, v44, v45, v46, v47
      v48, v49, v50, v51, v52, v53, v54, v55,
      v56, v57, v58, v59, v60, v61, v62, v63
    )) {}
    // Repeat 16 values as many times as necessary (usually for lookup tables)
    really_inline static simd8<uint8_t> repeat_16(
      uint8_t v0,  uint8_t v1,  uint8_t v2,  uint8_t v3,  uint8_t v4,  uint8_t v5,  uint8_t v6,  uint8_t v7,
      uint8_t v8,  uint8_t v9,  uint8_t v10, uint8_t v11, uint8_t v12, uint8_t v13, uint8_t v14, uint8_t v15
    ) {
      return simd8<uint8_t>(
        v0, v1, v2, v3, v4, v5, v6, v7,
        v8, v9, v10,v11,v12,v13,v14,v15,
        v0, v1, v2, v3, v4, v5, v6, v7,
        v8, v9, v10,v11,v12,v13,v14,v15,
        v0, v1, v2, v3, v4, v5, v6, v7,
        v8, v9, v10,v11,v12,v13,v14,v15,
        v0, v1, v2, v3, v4, v5, v6, v7,
        v8, v9, v10,v11,v12,v13,v14,v15
      );
    }

    // Saturated math
    really_inline simd8<uint8_t> saturating_add(const simd8<uint8_t> other) const { return _mm512_adds_epu8(*this, other); }
    really_inline simd8<uint8_t> saturating_sub(const simd8<uint8_t> other) const { return _mm512_subs_epu8(*this, other); }

    // Order-specific operations
    really_inline simd8<uint8_t> max(const simd8<uint8_t> other) const { return _mm512_max_epu8(*this, other); }
    really_inline simd8<uint8_t> min(const simd8<uint8_t> other) const { return _mm512_min_epu8(other, *this); }
    // Same as >, but only guarantees true is nonzero (< guarantees true = -1)
    really_inline simd8<uint8_t> gt_bits(const simd8<uint8_t> other) const { return this->saturating_sub(other); }
    // Same as <, but only guarantees true is nonzero (< guarantees true = -1)
    really_inline simd8<uint8_t> lt_bits(const simd8<uint8_t> other) const { return other.saturating_sub(*this); }
    really_inline simd8<bool> operator<=(const simd8<uint8_t> other) const { return other.max(*this) == other; }
    really_inline simd8<bool> operator>=(const simd8<uint8_t> other) const { return other.min(*this) == other; }
    really_inline simd8<bool> operator>(const simd8<uint8_t> other) const { return this->gt_bits(other).any_bits_set(); }
    really_inline simd8<bool> operator<(const simd8<uint8_t> other) const { return this->lt_bits(other).any_bits_set(); }

    // Bit-specific operations
    really_inline simd8<bool> bits_not_set() const { return *this == uint8_t(0); }
    really_inline simd8<bool> bits_not_set(simd8<uint8_t> bits) const { return (*this & bits).bits_not_set(); }
    really_inline simd8<bool> any_bits_set() const { return ~this->bits_not_set(); }
    really_inline simd8<bool> any_bits_set(simd8<uint8_t> bits) const { return ~this->bits_not_set(bits); }
    really_inline bool bits_not_set_anywhere() const { return _mm512_testz_si512(*this, *this); }
    really_inline bool any_bits_set_anywhere() const { return !bits_not_set_anywhere(); }
    really_inline bool bits_not_set_anywhere(simd8<uint8_t> bits) const { return _mm512_testz_si512(*this, bits); }
    really_inline bool any_bits_set_anywhere(simd8<uint8_t> bits) const { return !bits_not_set_anywhere(bits); }
    template<int N>
    really_inline simd8<uint8_t> shr() const { return simd8<uint8_t>(_mm512_srli_epi16(*this, N)) & uint8_t(0xFFu >> N); }
    template<int N>
    really_inline simd8<uint8_t> shl() const { return simd8<uint8_t>(_mm512_slli_epi16(*this, N)) & uint8_t(0xFFu << N); }
    // Get one of the bits and make a bitmask out of it.
    // e.g. value.get_bit<7>() gets the high bit
    template<int N>
    really_inline __mmask64 get_bit() const { return _mm512_test_epi8_mask(*this, 1 << N); }
  };

  template<typename T>
  struct simd8x64 {
    static const int NUM_CHUNKS = 64 / sizeof(simd8<T>);
    const simd8<T> chunks[NUM_CHUNKS];

    really_inline simd8x64() : chunks{simd8<T>(), simd8<T>()} {}
    really_inline simd8x64(const simd8<T> chunk0) : chunks{chunk0} {}
    really_inline simd8x64(const T ptr[64]) : chunks{simd8<T>::load(ptr)} {}

    template <typename F>
    static really_inline void each_index(F const& each) {
      each(0);
    }

    really_inline void store(T ptr[64]) const {
      this->chunks[0].store(ptr+sizeof(simd8<T>)*0);
    }

    template <typename F>
    really_inline void each(F const& each_chunk) const
    {
      each_chunk(this->chunks[0]);
    }

    template <typename R=bool, typename F>
    really_inline simd8x64<R> map(F const& map_chunk) const {
      return simd8x64<R>(
        map_chunk(this->chunks[0])
      );
    }

    template <typename R=bool, typename F>
    really_inline simd8x64<R> map(const simd8x64<uint8_t> b, F const& map_chunk) const {
      return simd8x64<R>(
        map_chunk(this->chunks[0], b.chunks[0])
      );
    }

    template <typename F>
    really_inline simd8<T> reduce(F const& reduce_pair) const {
      return reduce_pair(this->chunks[0]);
    }

    really_inline uint64_t to_bitmask() const {
      return this->chunks[0].to_bitmask();
    }

    really_inline simd8x64<T> bit_or(const T m) const {
      const simd8<T> mask = simd8<T>::splat(m);
      return this->map( [&](auto a) { return a | mask; } );
    }

    really_inline uint64_t eq(const T m) const {
      const simd8<T> mask = simd8<T>::splat(m);
      return this->map( [&](auto a) { return a == mask; } ).to_bitmask();
    }

    really_inline uint64_t lteq(const T m) const {
      const simd8<T> mask = simd8<T>::splat(m);
      return this->map( [&](auto a) { return a <= mask; } ).to_bitmask();
    }

  }; // struct simd8x64<T>

} // namespace simdjson::skylake_x::simd
UNTARGET_REGION

#endif // IS_X86_64
#endif // SIMDJSON_SKYLAKE_X_SIMD_H
