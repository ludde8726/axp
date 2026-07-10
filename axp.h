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
#define UNREACHABLE(x) assert(0 && x);
#else
#define AXP_ASSERT(x)
#define UNREACHABLE(x)
#endif

#define BASE 10

#define AXP_ZIV_DEFAULT_SAFETY_DIGITS 8
#define AXP_ZIV_DEFAULT_MAX_RETRIES 8

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
    AXP_ERR_UNINITIALIZED,
    AXP_ERR_FORMAT,
    AXP_ERR_WRITE,
} AXP_ErrorCode;

typedef struct {
    axp_size_t precision;
    AXP_ErrorCode err;
    char err_str[1024];
    
    bool fast_rounding;
    axp_size_t ziv_safety_digits;
    axp_size_t ziv_max_retries;
} AXP_Ctx;

typedef struct {
    axp_size_t size;
    axp_size_t capacity;
    axp_digit_t *digits; // Digits in little-endian order (least significant digit first.)
    uint8_t sign;        // One bit representing positve (0) or negative (1)
} AXP_Int;

typedef struct {
    axp_size_t size;
    axp_size_t capacity;
    axp_digit_t *digits; // Digits in little-endian order (least significant digit first.)
    uint8_t sign;        // One bit representing positve (0) or negative (1)
    axp_exp_t exponent;
} AXP_Float; // Number is represented as value = digits * 10^exp note that digits is an integer not a float so 1.23 would be represented as 123 * 10^-2

typedef int (*axp__print_writer_fn)(AXP_Ctx *, void *, const char *, size_t, int *);

typedef enum {
    AXP__TARGET_BUF,
    AXP__TARGET_FD,
} axp__target_type;

typedef struct {
    axp__target_type type;
    axp__print_writer_fn write_fn;

    union {
        FILE *fd;
        struct {
            char *buf;
            size_t buf_sz;
        };
    } as;
    
} axp__print_target;

/* -- ALLOCATION FUNCTIONS -- */
bool axp_initi(AXP_Ctx *ctx, AXP_Int *x, axp_size_t initial_capacity);
bool axp_initf(AXP_Ctx *ctx, AXP_Float *x);
bool axp_initf_ex(AXP_Ctx *ctx, AXP_Float *x, axp_size_t precision);

bool axp_realloci(AXP_Ctx *ctx, AXP_Int *x, axp_size_t size);
bool axp_reallocf(AXP_Ctx *ctx, AXP_Float *x, axp_size_t size);
bool axp_reallocf_round(AXP_Ctx *ctx, AXP_Float *x, axp_size_t size);

void axp_freei(AXP_Int *x);
void axp_freef(AXP_Float *x);

/* -- COPY FUNCTIONS -- */

// Creates a `AXP_Int` copy of `src` into `dst` where `dst` is an unitialized `AXP_Int`
// WARNING: Make sure that `src` and `dst` do NOT overlap!
// Return: true on sucess and false on faliure and sets ctx->err accordingly
bool axp_copyi(AXP_Ctx *ctx, AXP_Int *restrict dst, const AXP_Int *restrict src);
bool axp_copyi_ex(AXP_Ctx *ctx, AXP_Int *restrict dst, const AXP_Int *restrict src, axp_size_t capacity);

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
bool axp_copyf_ex_round(AXP_Ctx *ctx, AXP_Float *restrict dst, const AXP_Float *restrict src, axp_size_t precision);
// Creates a copy of `src` into `dst` where `dst` is an unitialized `AXP_Float`
// Note: This function will create an exact copy of `src` and will not respect the currently set
// precision in the context.
// WARNING: Make sure that `src` and `dst` do NOT overlap!
// Return: true on sucess and false on faliure and sets ctx->err accordingly
bool axp_copyf_exact(AXP_Ctx *ctx, AXP_Float *restrict dst, const AXP_Float *restrict src);

/* COMPARISON */
// Compares the absolute value of two `Cxp_Int`
// Sets res to: `1` if `x > y`, `0`if `x == y` and `-1` if `x < y`
bool axp_abs_cmpi(AXP_Ctx *ctx, const AXP_Int *x, const AXP_Int *y, int8_t *res);
bool axp_is_zeroi(AXP_Ctx *ctx, const AXP_Int *x, bool *res);

void axp_normalizef(AXP_Float *x);

void axp_roundf(AXP_Float *x, axp_size_t significant_digits);
bool axp_floorf(AXP_Ctx *ctx, const AXP_Float *x, AXP_Int *n, AXP_Float *f);

/* LOW LEVEL OPS */
axp_size_t axp__shli_digits(axp_digit_t *x_digits, axp_size_t x_sz, axp_size_t shift);
void axp_shli(AXP_Ctx *ctx, AXP_Int *x, axp_size_t shift);
axp_size_t axp__shri_digits(axp_digit_t *x_digits, axp_size_t x_sz, axp_size_t shift);
void axp_shri(AXP_Ctx *ctx, AXP_Int *x, axp_size_t shift);

axp_size_t axp__round_digits_into(axp_digit_t *src, axp_size_t src_sz, axp_digit_t *dst, axp_size_t dst_sz, axp_size_t *out_shift);

void axp_align_float_digits(AXP_Float *x, AXP_Float *y);

/* ADD FUNCTIONS */
// Returns the size of `res`
axp_size_t axp__add_digits(const axp_digit_t *x_digits, axp_size_t x_sz, const axp_digit_t *y_digits, axp_size_t y_sz, axp_digit_t *res);
bool axp_addi(AXP_Ctx *ctx, const AXP_Int *x, const AXP_Int *y, AXP_Int *res);
bool axp_addf(AXP_Ctx *ctx, const AXP_Float *x, const AXP_Float *y, AXP_Float *res);
bool axp_addf_ex(AXP_Ctx *ctx, const AXP_Float *x, const AXP_Float *y, AXP_Float *res, axp_size_t precision);

// NOTE: Assumes that `x > y`
axp_size_t axp__sub_digits(const axp_digit_t *x_digits, axp_size_t x_sz, const axp_digit_t *y_digits, axp_size_t y_sz, axp_digit_t *res);
bool axp_subi(AXP_Ctx *ctx, const AXP_Int *x, const AXP_Int *y, AXP_Int *res);
bool axp_subf(AXP_Ctx *ctx, const AXP_Float *x, const AXP_Float *y, AXP_Float *res);
bool axp_subf_ex(AXP_Ctx *ctx, const AXP_Float *x, const AXP_Float *y, AXP_Float *res, axp_size_t precision);

axp_size_t axp__mul_digits(const axp_digit_t *x_digits, axp_size_t x_sz, const axp_digit_t *y_digits, axp_size_t y_sz, axp_digit_t *res);
bool axp_muli(AXP_Ctx *ctx, const AXP_Int *x, const AXP_Int *y, AXP_Int *res);
bool axp_mulf(AXP_Ctx *ctx, const AXP_Float *x, const AXP_Float *y, AXP_Float *res);
bool axp_mulf_ex(AXP_Ctx *ctx, const AXP_Float *x, const AXP_Float *y, AXP_Float *res, axp_size_t precision);

axp_size_t axp__div_digits(axp_digit_t *x_digits, axp_size_t x_sz, axp_digit_t *y_digits, axp_size_t y_sz, axp_digit_t *res, axp_size_t *remainder_sz);
axp_size_t axp__div_digits_float(axp_digit_t *x_digits, axp_size_t x_sz, axp_digit_t *y_digits, axp_size_t y_sz, axp_digit_t *res, axp_size_t max_prec, axp_exp_t *exp_adjust);
axp_size_t axp__divf_uint(axp_digit_t *x_digits, axp_size_t x_sz, axp_exp_t x_exp, axp_size_t y, axp_digit_t *res, axp_size_t res_cap, axp_exp_t *res_exp);
bool axp_divi(AXP_Ctx *ctx, const AXP_Int *x, const AXP_Int *y, AXP_Int *res, AXP_Int *remainder);
bool axp_divf(AXP_Ctx *ctx, const AXP_Float *x, const AXP_Float *y, AXP_Float *res);
bool axp_divf_ex(AXP_Ctx *ctx, const AXP_Float *x, const AXP_Float *y, AXP_Float *res, axp_size_t precision);

axp_size_t axp__pow_digits(axp_digit_t *x_digits, axp_size_t x_sz, axp_size_t y, axp_digit_t *tmp_buf, axp_digit_t *res);
axp_size_t axp__pow_digits_float(axp_digit_t *x_digits, axp_size_t x_sz, axp_size_t y, axp_digit_t *tmp_buf, axp_digit_t *res, axp_size_t res_cap, axp_exp_t *exp_adj);
bool axp_powi(AXP_Ctx *ctx, AXP_Int *x, axp_size_t y, AXP_Int *res);
bool axp_powf(AXP_Ctx *ctx, AXP_Float *x, axp_exp_t y, AXP_Float *res);
bool axp_powf_ex(AXP_Ctx *ctx, AXP_Float *x, axp_exp_t y, AXP_Float *res, axp_size_t precision);

bool axp_e(AXP_Ctx *ctx, AXP_Float *res);
bool axp_e_ex(AXP_Ctx *ctx, AXP_Float *res, axp_size_t precision);

bool axp_expf(AXP_Ctx *ctx, const AXP_Float *x, AXP_Float *res);
bool axp_expf_ex(AXP_Ctx *ctx, const AXP_Float *x, AXP_Float *res, axp_size_t precision);
bool axp_expf_no_splitting(AXP_Ctx *ctx, const AXP_Float *x, AXP_Float *res, axp_size_t precision);

bool axp_lnf(AXP_Ctx *ctx, const AXP_Float *x, AXP_Float *res);
bool axp_lnf_ex(AXP_Ctx *ctx, const AXP_Float *x, AXP_Float *res, axp_size_t precision);

bool axp_powff(AXP_Ctx *ctx, AXP_Float *x, const AXP_Float *y, AXP_Float *res);
bool axp_powff_ex(AXP_Ctx *ctx, AXP_Float *x, const AXP_Float *y, AXP_Float *res, axp_size_t precision);

// Write AXP_Float to string, returns bytes written. If buf is NULL of buf_sz is 0 only the needed space will be returned.
size_t axp_itoa(AXP_Int *x, char *buf, size_t buf_sz);
char *axp_itoa_alloc(AXP_Ctx *ctx, AXP_Int *x);
bool axp_atoi(AXP_Ctx *ctx, const char *str, AXP_Int *x);

size_t axp_ftoa(AXP_Float *x, char *buf, size_t buf_sz);
char *axp_ftoa_alloc(AXP_Ctx *ctx, AXP_Float *x);
bool axp_atof(AXP_Ctx *ctx, const char *str, AXP_Float *x);

void axp_throw(AXP_Ctx *ctx, AXP_ErrorCode err_code, const char *fmt, ...) PRINTF_LIKE_WARNINGS(3, 4);
const char *axp_strerror(const AXP_Ctx *ctx);
void axp_error_reset(AXP_Ctx *ctx);

int axp__printf_core(AXP_Ctx *ctx, axp__print_target *target, const char *fmt, va_list *args);

int axp_vsnprintf(AXP_Ctx *ctx, char *buf, size_t buf_sz, const char *fmt, va_list *args);
int axp_vfprintf(AXP_Ctx *ctx, FILE *stream, const char *fmt, va_list *args);
int axp_snprintf(AXP_Ctx *ctx, char *buf, size_t buf_sz, const char *fmt, ...);
int axp_fprintf(AXP_Ctx *ctx, FILE *stream, const char *fmt, ...);
int axp_printf(AXP_Ctx *ctx, const char *fmt, ...);
#endif // _AXP_H
