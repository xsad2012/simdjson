// This file contains the common code every implementation uses for stage2
// It is intended to be included multiple times and compiled multiple times
// We assume the file in which it is include already includes
// "simdjson/stage2_build_tape.h" (this simplifies amalgation)

namespace stage2 {

#ifdef SIMDJSON_USE_COMPUTED_GOTO
typedef void* ret_address;
#define INIT_ADDRESSES() { &&array_begin, &&array_continue, &&error, &&finish, &&object_begin, &&object_continue }
#define GOTO(address) { goto *(address); }
#define CONTINUE(address) { goto *(address); }
#else
typedef char ret_address;
#define INIT_ADDRESSES() { '[', 'a', 'e', 'f', '{', 'o' };
#define GOTO(address)                 \
  {                                   \
    switch(address) {                 \
      case '[': goto array_begin;     \
      case 'a': goto array_continue;  \
      case 'e': goto error;           \
      case 'f': goto finish;          \
      case '{': goto object_begin;    \
      case 'o': goto object_continue; \
    }                                 \
  }
// For the more constrained pop_scope() situation
#define CONTINUE(address)             \
  {                                   \
    switch(address) {                 \
      case 'a': goto array_continue;  \
      case 'o': goto object_continue; \
      case 'f': goto finish;          \
    }                                 \
  }
#endif

struct unified_machine_addresses {
  ret_address array_begin;
  ret_address array_continue;
  ret_address error;
  ret_address finish;
  ret_address object_begin;
  ret_address object_continue;
};

#undef FAIL_IF
#define FAIL_IF(EXPR) { if (EXPR) { return addresses.error; } }

template<typename F>
really_inline bool with_space_terminated_copy(const uint8_t *buf, const size_t len, const F& f) {
  /**
  * We need to make a copy to make sure that the string is space terminated.
  * This is not about padding the input, which should already padded up
  * to len + SIMDJSON_PADDING. However, we have no control at this stage
  * on how the padding was done. What if the input string was padded with nulls?
  * It is quite common for an input string to have an extra null character (C string).
  * We do not want to allow 9\0 (where \0 is the null character) inside a JSON
  * document, but the string "9\0" by itself is fine. So we make a copy and
  * pad the input with spaces when we know that there is just one input element.
  * This copy is relatively expensive, but it will almost never be called in
  * practice unless you are in the strange scenario where you have many JSON
  * documents made of single atoms.
  */
  char *copy = static_cast<char *>(malloc(len + SIMDJSON_PADDING));
  if (copy == nullptr) {
    return true;
  }
  memcpy(copy, buf, len);
  memset(copy + len, ' ', SIMDJSON_PADDING);
  bool result = f(reinterpret_cast<const uint8_t*>(copy));
  free(copy);
  return result;
}

struct structural_parser {
  const uint8_t* const buf;
  const size_t len;
  ParsedJson &pj;
  size_t i; // next structural index
  size_t idx; // location of the structural character in the input (buf)
  uint8_t c;    // used to track the (structural) character we are looking at
  uint32_t depth = 0; // could have an arbitrary starting depth

  really_inline structural_parser(const uint8_t *_buf, size_t _len, ParsedJson &_pj, uint32_t _i = 0) : buf{_buf}, len{_len}, pj{_pj}, i{_i} {}

  WARN_UNUSED really_inline int set_error_code(ErrorValues error_code) {
    pj.error_code = error_code;
    return error_code;
  }

  really_inline char advance_char() {
    idx = pj.structural_indexes[i++];
    c = buf[idx];
    return c;
  }

  template<typename F>
  really_inline bool with_space_terminated_copy(const F& f) {
    return stage2::with_space_terminated_copy(buf, len, f);
  }

  WARN_UNUSED really_inline bool push_start_scope(ret_address continue_state, char type) {
    pj.containing_scope_offset[depth] = pj.get_current_loc();
    pj.ret_address[depth] = continue_state;
    depth++;
    pj.write_tape(0, type);
    return depth >= pj.depth_capacity;
  }

  WARN_UNUSED really_inline bool push_start_scope(ret_address continue_state) {
    return push_start_scope(continue_state, c);
  }

  WARN_UNUSED really_inline bool push_scope(ret_address continue_state) {
    pj.containing_scope_offset[depth] = pj.get_current_loc();
    pj.write_tape(0, c); // Do this as early as possible
    pj.ret_address[depth] = continue_state;
    depth++;
    return depth >= pj.depth_capacity;
  }

  WARN_UNUSED really_inline ret_address pop_scope() {
    // write our tape location to the header scope
    depth--;
    pj.write_tape(pj.containing_scope_offset[depth], c);
    pj.annotate_previous_loc(pj.containing_scope_offset[depth], pj.get_current_loc());
    return pj.ret_address[depth];
  }

  really_inline void pop_root_scope() {
    // write our tape location to the header scope
    // The root scope gets written *at* the previous location.
    depth--;
    pj.annotate_previous_loc(pj.containing_scope_offset[depth], pj.get_current_loc());
    pj.write_tape(pj.containing_scope_offset[depth], 'r');
  }

  really_inline void write_string() {
    pj.write_tape(pj.current_string_buf_loc - pj.string_buf.get(), '"');
    pj.current_string_buf_loc += sizeof(uint32_t) + *(uint32_t*)pj.current_string_buf_loc + 1;
  }

  really_inline void write_number() {
    pj.copy_number_tape();
  }

  really_inline void write_atom() {
    pj.write_tape(0, c);
  }

  WARN_UNUSED really_inline ret_address parse_value(const unified_machine_addresses &addresses, ret_address continue_state) {
    switch (c) {
    case '"':
      write_string();
      return continue_state;
    case 't': case 'f': case 'n':
      write_atom();
      return continue_state;
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    case '-':
      write_number();
      return continue_state;
    case '{':
      FAIL_IF( push_scope(continue_state) );
      return addresses.object_begin;
    case '[':
      FAIL_IF( push_scope(continue_state) );
      return addresses.array_begin;
    default:
      return addresses.error;
    }
  }

  WARN_UNUSED really_inline int finish() {
    // the string might not be NULL terminated.
    if ( i + 1 != pj.n_structural_indexes ) {
      return set_error_code(TAPE_ERROR);
    }
    pop_root_scope();
    if (depth != 0) {
      return set_error_code(TAPE_ERROR);
    }
    if (pj.containing_scope_offset[depth] != 0) {
      return set_error_code(TAPE_ERROR);
    }

    pj.valid = true;
    return set_error_code(SUCCESS);
  }

  WARN_UNUSED really_inline int error() {
    /* We do not need the next line because this is done by pj.init(),
    * pessimistically.
    * pj.is_valid  = false;
    * At this point in the code, we have all the time in the world.
    * Note that we know exactly where we are in the document so we could,
    * without any overhead on the processing code, report a specific
    * location.
    * We could even trigger special code paths to assess what happened
    * carefully,
    * all without any added cost. */
    if (depth >= pj.depth_capacity) {
      return set_error_code(DEPTH_ERROR);
    }
    switch (c) {
    case '"':
      return set_error_code(STRING_ERROR);
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '-':
      return set_error_code(NUMBER_ERROR);
    case 't':
      return set_error_code(T_ATOM_ERROR);
    case 'n':
      return set_error_code(N_ATOM_ERROR);
    case 'f':
      return set_error_code(F_ATOM_ERROR);
    default:
      return set_error_code(TAPE_ERROR);
    }
  }

  WARN_UNUSED really_inline int start(ret_address finish_state) {
    // Advance to the first character as soon as possible
    advance_char();
    // Push the root scope (there is always at least one scope)
    if (push_start_scope(finish_state, 'r')) {
      return set_error_code(DEPTH_ERROR);
    }
    return SUCCESS;
  }
};

// Redefine FAIL_IF to use goto since it'll be used inside the function now
#undef FAIL_IF
#define FAIL_IF(EXPR) { if (EXPR) { goto error; } }

WARN_UNUSED really_inline int parse_strings(const uint8_t *buf, ParsedJson &pj, uint32_t i=0) {
  bool had_error = false;
  for (; i < pj.n_structural_indexes - 1; i++) {
    uint32_t idx = pj.structural_indexes[i];
    if (buf[idx] == '"') {
      had_error |= !parse_string(buf, idx, pj.current_string_buf_loc);
    }
  }
  return had_error ? (pj.error_code = STRING_ERROR) : SUCCESS;
}

WARN_UNUSED really_inline int parse_numbers(const uint8_t *buf, const size_t len, ParsedJson &pj, uint32_t i=0) {
  bool had_error = false;

  // If we are the last thing, we need to make a space-terminated copy
  if (i == pj.n_structural_indexes - 2) {
    uint32_t idx = pj.structural_indexes[i];
    switch (buf[idx]) {
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        had_error |= with_space_terminated_copy(buf, len, [&](auto copy) { return !parse_number(copy, pj, idx, false); });
        break;
      case '-':
        had_error |= with_space_terminated_copy(buf, len, [&](auto copy) { return !parse_number(copy, pj, idx, true); });
        break;
    }
    i++;
  }

  for (; i < pj.n_structural_indexes - 1; i++) {
    uint32_t idx = pj.structural_indexes[i];
    switch (buf[idx]) {
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        had_error |= !parse_number(buf, pj, idx, false);
        break;
      case '-':
        had_error |= !parse_number(buf, pj, idx, true);
        break;
    }
  }

  return had_error ? (pj.error_code = NUMBER_ERROR) : SUCCESS;
}

WARN_UNUSED really_inline bool parse_atom(const uint8_t *buf, uint32_t idx) {
  switch (buf[idx]) {
    case 't':
      return !is_valid_true_atom(&buf[idx]);
      break;
    case 'f':
      return !is_valid_false_atom(&buf[idx]);
      break;
    case 'n':
      return !is_valid_null_atom(&buf[idx]);
      break;
    default:
      return false;
  }
  return false;
}

WARN_UNUSED really_inline int parse_atoms(const uint8_t *buf, const size_t len, ParsedJson &pj, uint32_t i=0) {
  bool had_error = false;

  // If we are the last thing, we need to make a space-terminated copy
  if (i == pj.n_structural_indexes - 2) {
    uint32_t idx = pj.structural_indexes[i];
    switch (buf[idx]) {
      case 't': case 'f': case 'n':
        had_error |= with_space_terminated_copy(buf, len, [&](auto copy) {
          return parse_atom(copy, idx);
        });
        break;
    }
    i++;
  }

  for (; i < pj.n_structural_indexes - 1; i++) {
    uint32_t idx = pj.structural_indexes[i];
    switch (buf[idx]) {
      case 't': case 'f': case 'n':
        had_error |= parse_atom(buf, idx);
        break;
    }
  }

  // TODO can't tell which error it is anymore, can we?
  return had_error ? (pj.error_code = N_ATOM_ERROR) : SUCCESS;
}

/************
 * The JSON is parsed to a tape, see the accompanying tape.md file
 * for documentation.
 ***********/
WARN_UNUSED  int
unified_machine(const uint8_t *buf, size_t len, ParsedJson &pj) {
  static constexpr unified_machine_addresses addresses = INIT_ADDRESSES();

  // Set up
  pj.init(); // sets is_valid to false
  if (len > pj.byte_capacity) {
    return pj.error_code = CAPACITY;
  }

  //
  // Parse values
  //
  if (parse_strings(buf, pj)) {
    return pj.error_code;
  }
  if (parse_numbers(buf, len, pj)) {
    return pj.error_code;
  }
  if (parse_atoms(buf, len, pj)) {
    return pj.error_code;
  }

  //
  // Parse structurals
  //
  pj.init(); // resets buf/tape locations
  structural_parser parser(buf, len, pj);
  if (parser.start(addresses.finish)) {
    return pj.error_code;
  }

  //
  // Read first value
  //
  switch (parser.c) {
  case '{':
    FAIL_IF( parser.push_start_scope(addresses.finish) );
    goto object_begin;
  case '[':
    FAIL_IF( parser.push_start_scope(addresses.finish) );
    goto array_begin;
  case '"':
    parser.write_string();
    goto finish;
  case 't': case 'f': case 'n':
    parser.write_atom();
    goto finish;
  case '0': case '1': case '2': case '3': case '4':
  case '5': case '6': case '7': case '8': case '9':
  case '-':
    parser.write_number();
    goto finish;
  default:
    goto error;
  }

//
// Object parser states
//
object_begin:
  parser.advance_char();
  switch (parser.c) {
  case '"':
    parser.write_string();
    goto object_key_state;
  case '}':
    goto scope_end; // could also go to object_continue
  default:
    goto error;
  }

object_key_state:
  FAIL_IF( parser.advance_char() != ':' );
  parser.advance_char();
  GOTO( parser.parse_value(addresses, addresses.object_continue) );

object_continue:
  switch (parser.advance_char()) {
  case ',':
    FAIL_IF( parser.advance_char() != '"' );
    parser.write_string();
    goto object_key_state;
  case '}':
    goto scope_end;
  default:
    goto error;
  }

scope_end:
  CONTINUE( parser.pop_scope() );

//
// Array parser states
//
array_begin:
  if (parser.advance_char() == ']') {
    goto scope_end; // could also go to array_continue
  }

main_array_switch:
  /* we call update char on all paths in, so we can peek at parser.c on the
   * on paths that can accept a close square brace (post-, and at start) */
  GOTO( parser.parse_value(addresses, addresses.array_continue) );

array_continue:
  switch (parser.advance_char()) {
  case ',':
    parser.advance_char();
    goto main_array_switch;
  case ']':
    goto scope_end;
  default:
    goto error;
  }

finish:
  return parser.finish();

error:
  return parser.error();
}

} // namespace stage2