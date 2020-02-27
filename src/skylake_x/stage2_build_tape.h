#ifndef SIMDJSON_SKYLAKE_X_STAGE2_BUILD_TAPE_H
#define SIMDJSON_SKYLAKE_X_STAGE2_BUILD_TAPE_H

#include "simdjson/portability.h"

#ifdef IS_X86_64

#include "skylake_x/implementation.h"
#include "skylake_x/stringparsing.h"
#include "skylake_x/numberparsing.h"

TARGET_SKYLAKE_X
namespace simdjson::skylake_x {

#include "generic/stage2_build_tape.h"
#include "generic/stage2_streaming_build_tape.h"

} // namespace simdjson
UNTARGET_REGION

#endif // IS_X86_64

#endif // SIMDJSON_SKYLAKE_X_STAGE2_BUILD_TAPE_H
