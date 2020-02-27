#ifndef SIMDJSON_SKYLAKE_X_STRINGPARSING_H
#define SIMDJSON_SKYLAKE_X_STRINGPARSING_H

#include "simdjson/portability.h"

#ifdef IS_X86_64

#include "skylake_x/simd.h"
#include "simdjson/common_defs.h"
#include "simdjson/parsedjson.h"
#include "jsoncharutils.h"
#include "skylake_x/intrinsics.h"
#include "skylake_x/bitmanipulation.h"

TARGET_SKYLAKE_X
namespace simdjson::skylake_x {

using namespace simd;

// Holds backslashes and quotes locations.
struct parse_string_helper {
  uint64_t bs_bits;
  uint64_t quote_bits;
  static const uint32_t BYTES_PROCESSED = 64;
};

really_inline parse_string_helper find_bs_bits_and_quote_bits(const uint8_t *src, uint8_t *dst) {
  // this can read up to 15 bytes beyond the buffer size, but we require
  // SIMDJSON_PADDING of padding
  static_assert(SIMDJSON_PADDING >= (parse_string_helper::BYTES_PROCESSED - 1));
  simd8<uint8_t> v(src);
  // store to dest unconditionally - we can overwrite the bits we don't like later
  v.store(dst);
  return {
      (uint64_t)(v == '\\').to_bitmask(),     // bs_bits
      (uint64_t)(v == '"').to_bitmask(), // quote_bits
  };
}

#include "generic/stringparsing.h"

} // namespace simdjson::skylake_x
UNTARGET_REGION

#endif // IS_X86_64

#endif
