#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "cxp.h"

bool cxp_initi(CXP_Ctx *ctx, CXP_Int *x)
{
    x->size = 0;
    x->capacity = ctx->precision;
    x->digits = calloc(ctx->precision, sizeof(cxp_digit_t));
    x->sign = 1;
    if (!x->digits) {
        return false;
    }
    return true;
}

bool cxp_initf(CXP_Ctx *ctx, CXP_Float *x)
{
    x->size = 0;
    x->capacity = ctx->precision;
    x->digits = calloc(ctx->precision, sizeof(cxp_digit_t));
    x->sign = 1;
    x->exponent = 0;
    return false;
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

const char *cxp_error_str(const CXP_Ctx *ctx) {
    if (*(ctx->err_str)) return ctx->err_str;
    return cxp_error_messages[ctx->err];
}
