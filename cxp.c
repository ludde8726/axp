#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "cxp.h"


bool cxp_initi(CXP_Ctx *ctx, CXP_Int *x, uint32_t initial_capacity)
{
    x->size = 0;
    x->capacity = initial_capacity;
    x->digits = calloc(initial_capacity, sizeof(cxp_digit_t));
    x->sign = 1;
    if (!x->digits) {
        cxp_throw(ctx, CXP_ERR_ALLOC, "Memory allocation failed, could not allocate %lu bytes.", initial_capacity*sizeof(cxp_digit_t));
        return false;
    }
    cxp_error_reset(ctx);
    return true;
}

bool cxp_initf(CXP_Ctx *ctx, CXP_Float *x)
{
    return cxp_initf_ex(ctx, x, ctx->precision);
}

bool cxp_initf_ex(CXP_Ctx *ctx, CXP_Float *x, uint32_t precision)
{
    x->size = 0;
    x->capacity = precision;
    x->digits = calloc(precision, sizeof(cxp_digit_t));
    x->sign = 1;
    x->exponent = 0;
    if (!x->digits) {
        cxp_throw(ctx, CXP_ERR_ALLOC, "Memory allocation failed, could not allocate %lu bytes.", precision*sizeof(cxp_digit_t));
        return false;
    }
    cxp_error_reset(ctx);
    return true;
}

bool cxp_copyi(CXP_Ctx *ctx, CXP_Int *restrict dst, const CXP_Int *restrict src)
{
    CXP_ASSERT(!dst->digits);
    if (!cxp_initi(ctx, dst, dst->capacity)) return false; // Sets the capcity for us
    dst->size = src->size;
    dst->sign = src->sign;
    memcpy(dst->digits, src->digits, src->size*sizeof(cxp_digit_t));
    return true; // We do not need to reset error here since init already does and nothing we do after can raise any errors
}

bool cxp_copyf(CXP_Ctx *ctx, CXP_Float *restrict dst, const CXP_Float *restrict src)
{
    return cxp_copyf_ex(ctx, dst, src, ctx->precision); // We do not need to reset error here since init already does and nothing we do after can raise any errors
}

bool cxp_copyf_ex(CXP_Ctx *ctx, CXP_Float *restrict dst, const CXP_Float *restrict src, uint32_t precision)
{
    CXP_ASSERT(!dst->digits);
    if (!cxp_initf_ex(ctx, dst, precision)) return false; // Sets the capacity for us
    dst->sign = src->sign;
    dst->exponent = src->exponent;
    if (src->size > precision) {
        memcpy(dst->digits, src->digits + (src->size - precision), precision * sizeof(cxp_digit_t));
        dst->size = precision;
        return true;
    }
    memcpy(dst->digits, src->digits, src->size * sizeof(cxp_digit_t));
    dst->size = src->size;
    return true; // We do not need to reset error here since init already does and nothing we do after can raise any errors
}

bool cxp_copyf_exact(CXP_Ctx *ctx, CXP_Float *restrict dst, const CXP_Float *restrict src)
{
    CXP_ASSERT(!dst->digits);
    if (!cxp_initf_ex(ctx, dst, src->capacity)) return false; // Sets the capacity for us
    dst->sign = src->sign;
    dst->exponent = src->exponent;
    dst->size = src->size;
    memcpy(dst->digits, src->digits, src->size * sizeof(cxp_digit_t));
    return true; // We do not need to reset error here since init already does and nothing we do after can raise any errors
}

void cxp_throw(CXP_Ctx *ctx, CXP_ErrorCode err_code, const char *fmt, ...) {
    ctx->err = err_code;
    va_list args;
    va_start(args, fmt);
    vsnprintf(ctx->err_str, sizeof(ctx->err_str), fmt, args);
    va_end(args);
}

static const char *cxp_error_messages[] = {
    [CXP_OK] = "No error",
    [CXP_ERR_ALLOC] = "Memory allocation failed",
    [CXP_ERR_DIV_ZERO] = "Division by zero",
    [CXP_ERR_OVERFLOW] = "Digit overflow",
    [CXP_ERR_ROUNDING] = "Rounding error"
};


const char *cxp_error_str(const CXP_Ctx *ctx)
{
    if (*(ctx->err_str)) return ctx->err_str;
    return cxp_error_messages[ctx->err];
}

void cxp_error_reset(CXP_Ctx *ctx)
{
    ctx->err = CXP_OK;
    ctx->err_str[0] = '\0';
}
