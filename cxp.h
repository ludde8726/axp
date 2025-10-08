#ifndef _CXP_H
#define _CXP_H

#include <stdint.h>
#include <stdbool.h>

#if defined(__GNUC__) || defined(__clang__)
#define PRINTF_LIKE_WARNINGS(string_idx, arguments_idx_start) __attribute__((format(printf, string_idx, arguments_idx_start)))
#else
#define PRINTF_LIKE_WARNINGS(string_idx, arguments_idx_start)
#endif

#ifdef CXP_DEBUG_ASSERTS
#include <assert.h>
#define CXP_ASSERT(x) assert(x)
#else
#define CXP_ASSERT(x)
#endif

typedef uint8_t cxp_digit_t;
typedef uint32_t cxp_size_t;
typedef int64_t cxp_exp_t;

typedef enum {
    CXP_OK = 0,
    CXP_ERR_ALLOC,
    CXP_ERR_DIV_ZERO,
    CXP_ERR_OVERFLOW,
    CXP_ERR_ROUNDING,
    CXP_ERR_PARSE,
} CXP_ErrorCode;

typedef struct {
    cxp_size_t precision;
    CXP_ErrorCode err;
    char err_str[1024];
} CXP_Ctx;

typedef struct {
    cxp_size_t size;
    cxp_size_t capacity;
    cxp_digit_t *digits;
    uint8_t sign;
} CXP_Int;

typedef struct {
    cxp_size_t size;
    cxp_size_t capacity;
    cxp_digit_t *digits;
    uint8_t sign;
    int64_t exponent;
} CXP_Float;

/* -- INIT FUNCTIONS -- */
bool cxp_initi(CXP_Ctx *ctx, CXP_Int *x, cxp_size_t initial_capacity);
bool cxp_initf(CXP_Ctx *ctx, CXP_Float *x);
bool cxp_initf_ex(CXP_Ctx *ctx, CXP_Float *x, cxp_size_t precision);

bool cxp_initi_from_str(CXP_Ctx *ctx, const char *str, CXP_Int *x);

void cxp_freei(CXP_Int *x);
void cxp_freef(CXP_Float *x);

/* -- COPY FUNCTIONS -- */

// Creates a `CXP_Int` copy of `src` into `dst` where `dst` is an unitialized `CXP_Int`
// WARNING: Make sure that `src` and `dst` do NOT overlap!
// Return: true on sucess and false on faliure and sets ctx->err accordingly
bool cxp_copyi(CXP_Ctx *ctx, CXP_Int *restrict dst, const CXP_Int *restrict src);

// Creates a copy of `src` into `dst` where `dst` is an unitialized `CXP_Float`
// Note: This function will use the precision from the context, which may result in loss of precision.
// For an exact copy use `cxp_copyf_exact` or `cxp_copyf_ex` for copying to a specified precision.
// WARNING: Make sure that `src` and `dst` do NOT overlap!
// Return: true on sucess and false on faliure and sets ctx->err accordingly
bool cxp_copyf(CXP_Ctx *ctx, CXP_Float *restrict dst, const CXP_Float *restrict src);
// Creates a copy of `src` into `dst` where `dst` is an unitialized `CXP_Float`
// Note: This function will use the precision from the supplied variable, if this variable is less
// than the size of `src` this will result in a loss of precision.
// For an exact copy use `cxp_copyf_exact`
// WARNING: Make sure that `src` and `dst` do NOT overlap!
// Return: true on sucess and false on faliure and sets ctx->err accordingly
bool cxp_copyf_ex(CXP_Ctx *ctx, CXP_Float *restrict dst, const CXP_Float *restrict src, cxp_size_t precision);
// Creates a copy of `src` into `dst` where `dst` is an unitialized `CXP_Float`
// Note: This function will create an exact copy of `src` and will not respect the currently set
// precision in the context.
// WARNING: Make sure that `src` and `dst` do NOT overlap!
// Return: true on sucess and false on faliure and sets ctx->err accordingly
bool cxp_copyf_exact(CXP_Ctx *ctx, CXP_Float *restrict dst, const CXP_Float *restrict src);

/* COMPARISON */
// Compares the absolute value of two `Cxp_Int`
// Return: `1` if `x > y`, `0`if `x == y` and `-1` if `x < y`
int cxp_abs_cmpi(CXP_Ctx *ctx, const CXP_Int *x, const CXP_Int *y);

/* ADD FUNCTIONS */
// Returns the size of `res`
cxp_size_t cxp__add_digits(const cxp_digit_t *x_digits, cxp_size_t x_sz, const cxp_digit_t *y_digits, cxp_size_t y_sz, cxp_digit_t *res);
bool cxp_addi(CXP_Ctx *ctx, const CXP_Int *x, const CXP_Int *y, CXP_Int *res);

// NOTE: Assumes that `x > y`
cxp_size_t cxp__sub_digits(const cxp_digit_t *x_digits, cxp_size_t x_sz, const cxp_digit_t *y_digits, cxp_size_t y_sz, cxp_digit_t *res);
bool cxp_subi(CXP_Ctx *ctx, const CXP_Int *x, const CXP_Int *y, CXP_Int *res);

void cxp_throw(CXP_Ctx *ctx, CXP_ErrorCode err_code, const char *fmt, ...) PRINTF_LIKE_WARNINGS(3, 4);
const char *cxp_strerror(const CXP_Ctx *ctx);
void cxp_error_reset(CXP_Ctx *ctx);

#endif // _CXP_H
