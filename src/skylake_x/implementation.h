#ifndef __SIMDJSON_SKYLAKE_X_IMPLEMENTATION_H
#define __SIMDJSON_SKYLAKE_X_IMPLEMENTATION_H

#include "simdjson/portability.h"

#ifdef IS_X86_64

#include "simdjson/implementation.h"
#include "simdjson/isadetection.h"

namespace simdjson::skylake_x {

class implementation final : public simdjson::implementation {
public:
  really_inline implementation() : simdjson::implementation(
      "skylake_x",
      "Intel/AMD AVX512 F/VL/BW (Skylake-X and up)",
      instruction_set::PCLMULQDQ | instruction_set::BMI1      | instruction_set::BMI2 |
      instruction_set::AVX512_F  | instruction_set::AVX512_VL | instruction_set::AVX512_BW
  ) {}
  WARN_UNUSED error_code parse(const uint8_t *buf, size_t len, document::parser &parser) const noexcept final;
  WARN_UNUSED error_code stage1(const uint8_t *buf, size_t len, document::parser &parser, bool streaming) const noexcept final;
  WARN_UNUSED error_code stage2(const uint8_t *buf, size_t len, document::parser &parser) const noexcept final;
  WARN_UNUSED error_code stage2(const uint8_t *buf, size_t len, document::parser &parser, size_t &next_json) const noexcept final;
};

} // namespace simdjson::skylake_x

#endif // IS_X86_64

#endif // __SIMDJSON_SKYLAKE_X_IMPLEMENTATION_H