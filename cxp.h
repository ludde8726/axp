#ifndef _CXP_H
#define _CXP_H

#include <stdint.h>
#include <stdbool.h>

#if defined(__GNUC__) || defined(__clang__)
#define PRINTF_LIKE_WARNINGS(string_idx, arguments_idx_start) __attribute__((format(printf, string_idx, arguments_idx_start)))
#else
#define PRINTF_LIKE_WARNINGS(string_idx, arguments_idx_start)
#endif

typedef uint8_t cxp_digit_t;
typedef uint32_t cxp_size_t;
typedef int64_t cxp_exp_t;

typedef enum {
    CXP_OK,
    CXP_ERR_ALLOC,
    CXP_ERR_DIV_ZERO,
    CXP_ERR_OVERFLOW,
    CXP_ERR_ROUNDING,
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

bool cxp_initi(CXP_Ctx *ctx, CXP_Int *x);
bool cxp_initf(CXP_Ctx *ctx, CXP_Float *x);

void cxp_throw(CXP_Ctx *ctx, CXP_ErrorCode err_code, const char *fmt, ...) PRINTF_LIKE_WARNINGS(3, 4);
const char *cxp_error_str(const CXP_Ctx *ctx);

#endif // _CXP_H
