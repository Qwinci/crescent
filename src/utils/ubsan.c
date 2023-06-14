#include "types.h"
#include "stdio.h"

static void message(const char* msg) {
	kprintf_nolock("%s", msg);
}

static void abort_with_message(const char* msg) {
	panic("%s", msg);
}

#define MAX_CALLER_PCS 20
static atomic_usize CALLER_PCS[MAX_CALLER_PCS];
static atomic_u32 CALLER_PCS_SIZE;

__attribute__((noinline)) static bool report_this_error(uintptr_t caller) {
	if (caller == 0) {
		return false;
	}

	while (true) {
		unsigned sz = atomic_load_explicit(&CALLER_PCS_SIZE, memory_order_relaxed);
		if (sz > MAX_CALLER_PCS) {
			return false; // early exit
		}
		// when sz==kMaxCallerPcs print "too many errors", but only when cmpxchg
		// succeeds in order to not print it multiple times.
		if (sz > 0 && sz < MAX_CALLER_PCS) {
			uintptr_t p;
			for (unsigned i = 0; i < sz; ++i) {
				p = atomic_load_explicit(&CALLER_PCS[i], memory_order_relaxed);
				if (p == 0) {
					break;  // Concurrent update.
				}
				if (p == caller) {
					return false;
				}
			}
			if (p == 0) {
				continue;  // FIXME: yield?
			}
		}

		if (!atomic_compare_exchange_strong(&CALLER_PCS_SIZE, &sz, sz + 1)) {
			continue;  // Concurrent update! Try again from the start.
		}

		if (sz == MAX_CALLER_PCS) {
			message("ubsan: too many errors\n");
			return false;
		}
		atomic_store_explicit(&CALLER_PCS[sz], caller, memory_order_relaxed);
		return true;
	}
}

__attribute__((noinline)) static void decorate_msg(char *buf,
												   uintptr_t caller) {
	// print the address by nibbles
	for (unsigned shift = sizeof(uintptr_t) * 8; shift;) {
		shift -= 4;
		unsigned nibble = (caller >> shift) & 0xf;
		*(buf++) = nibble < 10 ? nibble + '0' : nibble - 10 + 'a';
	}
	// finish the message
	buf[0] = '\n';
	buf[1] = '\0';
}

#define SANITIZER_WORDSIZE 64
#define GET_CALLER_PC() (uintptr_t) __builtin_return_address(0)

// How many chars we need to reserve to print an address.
#define kAddrBuf (SANITIZER_WORDSIZE / 4)
#define MSG_TMPL(msg) "ubsan: " msg " by 0x"
#define MSG_TMPL_END(buf, msg) (buf + sizeof(MSG_TMPL(msg)) - 1)
// Reserve an additional byte for '\n'.
#define MSG_BUF_LEN(msg) (sizeof(MSG_TMPL(msg)) + kAddrBuf + 1)

#define HANDLER_RECOVER(name, msg)                               \
	void __ubsan_handle_##name##_minimal() {             \
    uintptr_t caller = GET_CALLER_PC();                  \
    if (!report_this_error(caller)) return;                      \
    char msg_buf[MSG_BUF_LEN(msg)] = MSG_TMPL(msg);              \
    decorate_msg(MSG_TMPL_END(msg_buf, msg), caller);            \
    message(msg_buf);                                            \
  }

#define HANDLER_NORECOVER(name, msg)                             \
	void __ubsan_handle_##name##_minimal_abort() {       \
    char msg_buf[MSG_BUF_LEN(msg)] = MSG_TMPL(msg);              \
    decorate_msg(MSG_TMPL_END(msg_buf, msg), GET_CALLER_PC());   \
    message(msg_buf);                                            \
    abort_with_message(msg_buf);                                 \
  }

#define HANDLER(name, msg)                                       \
  HANDLER_RECOVER(name, msg)                                     \
  HANDLER_NORECOVER(name, msg)

HANDLER(type_mismatch, "type-mismatch")
HANDLER(alignment_assumption, "alignment-assumption")
HANDLER(add_overflow, "add-overflow")
HANDLER(sub_overflow, "sub-overflow")
HANDLER(mul_overflow, "mul-overflow")
HANDLER(negate_overflow, "negate-overflow")
HANDLER(divrem_overflow, "divrem-overflow")
HANDLER(shift_out_of_bounds, "shift-out-of-bounds")
HANDLER(out_of_bounds, "out-of-bounds")
HANDLER_RECOVER(builtin_unreachable, "builtin-unreachable")
HANDLER_RECOVER(missing_return, "missing-return")
HANDLER(vla_bound_not_positive, "vla-bound-not-positive")
HANDLER(float_cast_overflow, "float-cast-overflow")
HANDLER(load_invalid_value, "load-invalid-value")
HANDLER(invalid_builtin, "invalid-builtin")
HANDLER(invalid_objc_cast, "invalid-objc-cast")
HANDLER(function_type_mismatch, "function-type-mismatch")
HANDLER(implicit_conversion, "implicit-conversion")
HANDLER(nonnull_arg, "nonnull-arg")
HANDLER(nonnull_return, "nonnull-return")
HANDLER(nullability_arg, "nullability-arg")
HANDLER(nullability_return, "nullability-return")
HANDLER(pointer_overflow, "pointer-overflow")
HANDLER(cfi_check_fail, "cfi-check-fail")
