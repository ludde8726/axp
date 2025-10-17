#ifndef _AXP_H
#define _AXP_H

#include <stdint.h>
#include <stdbool.h>

#if defined(__GNUC__) || defined(__clang__)
#define PRINTF_LIKE_WARNINGS(string_idx, arguments_idx_start) __attribute__((format(printf, string_idx, arguments_idx_start)))
#else
#define PRINTF_LIKE_WARNINGS(string_idx, arguments_idx_start)
#endif

#ifdef AXP_DEBUG_ASSERTS
#include <assert.h>
#define AXP_ASSERT(x) assert(x)
#else
#define AXP_ASSERT(x)
#endif

typedef uint8_t axp_digit_t;
typedef uint32_t axp_size_t;
typedef int64_t axp_exp_t;

typedef enum {
    AXP_OK = 0,
    AXP_ERR_ALLOC,
    AXP_ERR_DIV_ZERO,
    AXP_ERR_OVERFLOW,
    AXP_ERR_ROUNDING,
    AXP_ERR_PARSE,
} AXP_ErrorCode;

typedef struct {
    axp_size_t precision;
    AXP_ErrorCode err;
    char err_str[1024];
} AXP_Ctx;

typedef struct {
    axp_size_t size;
    axp_size_t capacity;
    axp_digit_t *digits;
    uint8_t sign;
} AXP_Int;

typedef struct {
    axp_size_t size;
    axp_size_t capacity;
    axp_digit_t *digits;
    uint8_t sign;
    int64_t exponent;
} AXP_Float;

/* -- ALLOCATION FUNCTIONS -- */
bool axp_initi(AXP_Ctx *ctx, AXP_Int *x, axp_size_t initial_capacity);
bool axp_initf(AXP_Ctx *ctx, AXP_Float *x);
bool axp_initf_ex(AXP_Ctx *ctx, AXP_Float *x, axp_size_t precision);

bool axp_realloci(AXP_Ctx *ctx, AXP_Int *x, axp_size_t size);
bool axp_reallocf(AXP_Ctx *ctx, AXP_Float *x, axp_size_t size);

bool axp_initi_from_str(AXP_Ctx *ctx, const char *str, AXP_Int *x);

void axp_freei(AXP_Int *x);
void axp_freef(AXP_Float *x);

/* -- COPY FUNCTIONS -- */

// Creates a `AXP_Int` copy of `src` into `dst` where `dst` is an unitialized `AXP_Int`
// WARNING: Make sure that `src` and `dst` do NOT overlap!
// Return: true on sucess and false on faliure and sets ctx->err accordingly
bool axp_copyi(AXP_Ctx *ctx, AXP_Int *restrict dst, const AXP_Int *restrict src);

// Creates a copy of `src` into `dst` where `dst` is an unitialized `AXP_Float`
// Note: This function will use the precision from the context, which may result in loss of precision.
// For an exact copy use `axp_copyf_exact` or `axp_copyf_ex` for copying to a specified precision.
// WARNING: Make sure that `src` and `dst` do NOT overlap!
// Return: true on sucess and false on faliure and sets ctx->err accordingly
bool axp_copyf(AXP_Ctx *ctx, AXP_Float *restrict dst, const AXP_Float *restrict src);
// Creates a copy of `src` into `dst` where `dst` is an unitialized `AXP_Float`
// Note: This function will use the precision from the supplied variable, if this variable is less
// than the size of `src` this will result in a loss of precision.
// For an exact copy use `axp_copyf_exact`
// WARNING: Make sure that `src` and `dst` do NOT overlap!
// Return: true on sucess and false on faliure and sets ctx->err accordingly
bool axp_copyf_ex(AXP_Ctx *ctx, AXP_Float *restrict dst, const AXP_Float *restrict src, axp_size_t precision);
// Creates a copy of `src` into `dst` where `dst` is an unitialized `AXP_Float`
// Note: This function will create an exact copy of `src` and will not respect the currently set
// precision in the context.
// WARNING: Make sure that `src` and `dst` do NOT overlap!
// Return: true on sucess and false on faliure and sets ctx->err accordingly
bool axp_copyf_exact(AXP_Ctx *ctx, AXP_Float *restrict dst, const AXP_Float *restrict src);

/* COMPARISON */
// Compares the absolute value of two `Cxp_Int`
// Return: `1` if `x > y`, `0`if `x == y` and `-1` if `x < y`
int axp_abs_cmpi(AXP_Ctx *ctx, const AXP_Int *x, const AXP_Int *y);

/* ADD FUNCTIONS */
// Returns the size of `res`
axp_size_t axp__add_digits(const axp_digit_t *x_digits, axp_size_t x_sz, const axp_digit_t *y_digits, axp_size_t y_sz, axp_digit_t *res);
bool axp_addi(AXP_Ctx *ctx, const AXP_Int *x, const AXP_Int *y, AXP_Int *res);

// NOTE: Assumes that `x > y`
axp_size_t axp__sub_digits(const axp_digit_t *x_digits, axp_size_t x_sz, const axp_digit_t *y_digits, axp_size_t y_sz, axp_digit_t *res);
bool axp_subi(AXP_Ctx *ctx, const AXP_Int *x, const AXP_Int *y, AXP_Int *res);

void axp_throw(AXP_Ctx *ctx, AXP_ErrorCode err_code, const char *fmt, ...) PRINTF_LIKE_WARNINGS(3, 4);
const char *axp_strerror(const AXP_Ctx *ctx);
void axp_error_reset(AXP_Ctx *ctx);

#endif // _AXP_H
