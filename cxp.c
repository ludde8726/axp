#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "cxp.h"


bool cxp_initi(CXP_Ctx *ctx, CXP_Int *x, cxp_size_t initial_capacity)
{
    x->size = 1;
    x->capacity = initial_capacity;
    x->digits = calloc(initial_capacity, sizeof(cxp_digit_t));
    x->sign = 0;
    if (!x->digits) {
        cxp_throw(ctx, CXP_ERR_ALLOC, "Memory allocation failed, could not allocate %lu bytes in `cxp_initi`.", initial_capacity*sizeof(cxp_digit_t));
        return false;
    }
    cxp_error_reset(ctx);
    return true;
}

bool cxp_initf(CXP_Ctx *ctx, CXP_Float *x)
{
    return cxp_initf_ex(ctx, x, ctx->precision);
}

bool cxp_initf_ex(CXP_Ctx *ctx, CXP_Float *x, cxp_size_t precision)
{
    x->size = 0;
    x->capacity = precision;
    x->digits = calloc(precision, sizeof(cxp_digit_t));
    x->sign = 0;
    x->exponent = 0;
    if (!x->digits) {
        cxp_throw(ctx, CXP_ERR_ALLOC, "Memory allocation failed, could not allocate %lu bytes in `cxp_initf_ex`.", precision*sizeof(cxp_digit_t));
        return false;
    }
    cxp_error_reset(ctx);
    return true;
}

bool cxp_realloci(CXP_Ctx *ctx, CXP_Int *x, cxp_size_t size)
{
    if (size < x->size) {
        cxp_size_t diff = x->size - size;
        memmove(x->digits, x->digits + diff, size * sizeof(cxp_digit_t));
        x->size = size;
    }
    
    cxp_digit_t *new_digits = realloc(x->digits, size * sizeof(cxp_digit_t));
    if (!new_digits) {
        free(x->digits);
        cxp_throw(ctx, CXP_ERR_ALLOC, "Memory allocation failed, could not reallocate %lu bytes in `cxp_initf_ex`.", size*sizeof(cxp_digit_t));
        return false;
    }
    x->digits = new_digits;
    x->capacity = size;
    return true;
}

bool cxp_reallocf(CXP_Ctx *ctx, CXP_Float *x, cxp_size_t size)
{
    if (size < x->size) {
        cxp_size_t diff = x->size - size;
        memmove(x->digits, x->digits + diff, size * sizeof(cxp_digit_t));
        x->size = size;
    }
    
    cxp_digit_t *new_digits = realloc(x->digits, size * sizeof(cxp_digit_t));
    if (!new_digits) {
        free(x->digits);
        cxp_throw(ctx, CXP_ERR_ALLOC, "Memory allocation failed, could not reallocate %lu bytes in `cxp_initf_ex`.", size*sizeof(cxp_digit_t));
        return false;
    }
    x->digits = new_digits;
    x->capacity = size;
    return true;
}

bool cxp_initi_from_str(CXP_Ctx *ctx, const char *str, CXP_Int *x)
{
    if (!str || !*str) {
        cxp_throw(ctx, CXP_ERR_PARSE, "Could not parse empty string or null pointer as CXP_Int.");
        return false;
    }
    uint8_t sign = 0;
    cxp_size_t res_sz = 0;
    cxp_size_t allocated = 4;
    cxp_digit_t *res_digits = calloc(4, sizeof(cxp_digit_t));
    if (!res_digits) {
        cxp_throw(ctx, CXP_ERR_ALLOC, "Memory allocation failed, could not allocate %lu bytes in `cxp_initi_from_str`.", allocated*sizeof(cxp_digit_t));
        return false;
    }

    while(isspace(*str)) str++;

    if (*str == '-') {
        sign = 1;
        str++;
    }

    while (*str) {
        const char chr = *str;
        if (isspace(chr)) {
            str++;
            continue;
        }

        if (!isdigit(chr)) {
            cxp_throw(ctx, CXP_ERR_PARSE, "String parsing failed, could not parse '%c' as a digit", chr);
            free(res_digits);
            return false;
        }

        if (res_sz >= allocated) {
            cxp_digit_t *tmp = realloc(res_digits, allocated*2*sizeof(cxp_digit_t));
            if (!tmp) {
                free(res_digits);
                cxp_throw(ctx, CXP_ERR_ALLOC, "Memory allocation failed, could not allocate %lu bytes in `cxp_initi_from_str`.", allocated*sizeof(cxp_digit_t));
                return false;
            }
            res_digits = tmp;
            allocated *= 2;
        }
        printf("%d, %d\n", res_sz, allocated);
        res_digits[res_sz++] = (cxp_digit_t)(chr - '0');
        str++;
    }

    // Must be a better way to do this without reversing, but works for now
    for (cxp_size_t i = 0; i < res_sz / 2; ++i) {
        cxp_digit_t tmp = res_digits[i]; // Digit at index i from the left
        res_digits[i] = res_digits[res_sz - 1 - i]; // Digit from index i from the right
        res_digits[res_sz - 1 - i] = tmp; // Swap
    }
    x->sign = sign;
    x->digits = res_digits;
    x->size = res_sz;
    x->capacity = allocated;
    return true;
}

void cxp_freei(CXP_Int *x)
{
    CXP_ASSERT(x->digits);
    free(x->digits);
}

void cxp_freef(CXP_Float *x)
{
    CXP_ASSERT(x->digits);
    free(x->digits);
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

bool cxp_copyf_ex(CXP_Ctx *ctx, CXP_Float *restrict dst, const CXP_Float *restrict src, cxp_size_t precision)
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

int cxp_abs_cmpi(CXP_Ctx *ctx, const CXP_Int *x, const CXP_Int *y)
{
    // Maybe don't pass ctx to this function, however i like the consistancy.
    (void)ctx;
    if (x->size > y->size) return 1;
    if (x->size < y->size) return -1;
    for (cxp_size_t i = x->size; i > 0; i--) {
        if (x->digits[i-1] > y->digits[i-1]) return 1;
        if (x->digits[i-1] < y->digits[i-1]) return -1;
    }
    return 0;
}

cxp_size_t cxp__add_digits(const cxp_digit_t *x_digits, cxp_size_t x_sz, const cxp_digit_t *y_digits, cxp_size_t y_sz, cxp_digit_t *res)
{
    cxp_digit_t carry = 0;
    cxp_size_t res_sz = 0;
    cxp_size_t max_sz = (x_sz > y_sz) ? x_sz : y_sz;
    for (cxp_size_t i = 0; (i < max_sz) || carry; i++) {
        cxp_digit_t sum = carry;
        if (i < x_sz) sum += x_digits[i];
        if (i < y_sz) sum += y_digits[i];
        res[i] = sum % 10;
        carry = sum / 10;
        res_sz++;
    }
    return res_sz;
}

bool cxp_addi(CXP_Ctx *ctx, const CXP_Int *x, const CXP_Int *y, CXP_Int *res)
{
    cxp_size_t max_sz = (x->size > y->size) ? x->size + 1 : y->size + 1;
    if (!cxp_initi(ctx, res, max_sz)) return false;
    if (x->sign != y->sign) {
        if (x->sign) {
            // This is quite repetetive, maybe refactor...
            const CXP_Int *larger = y;
            const CXP_Int *smaller = x;
            cxp_digit_t res_sign = 0;
            int cmp = cxp_abs_cmpi(ctx, y, x);
            if (cmp == 0) return true;
            else if (cmp < 0) {
                res_sign = 1;
                larger = x;
                smaller = y;
            }
            cxp_size_t res_sz = cxp__sub_digits(larger->digits, larger->size, smaller->digits, smaller->size, res->digits);
            res->size = res_sz;
            res->sign = res_sign;
            return true;
        } else {
            const CXP_Int *larger = x;
            const CXP_Int *smaller = y;
            cxp_digit_t res_sign = 0;
            int cmp = cxp_abs_cmpi(ctx, x, y);
            if (cmp == 0) return true;
            else if (cmp < 0) {
                res_sign = 1;
                larger = x;
                smaller = y;
            }
            cxp_size_t res_sz = cxp__sub_digits(larger->digits, larger->size, smaller->digits, smaller->size, res->digits);
            res->size = res_sz;
            res->sign = res_sign;
            return true;
        }
    }
    
    
    cxp_size_t res_sz = cxp__add_digits(x->digits, x->size, y->digits, y->size, res->digits);
    res->size = res_sz;
    res->sign = x->sign;
    return true;
}

cxp_size_t cxp__sub_digits(const cxp_digit_t *x_digits, cxp_size_t x_sz, const cxp_digit_t *y_digits, cxp_size_t y_sz, cxp_digit_t *res)
{
    cxp_digit_t borrow = 0;
    cxp_size_t res_sz = x_sz;
    for (cxp_size_t i = 0; i < x_sz; i++) {
        int diff = x_digits[i] - borrow;
        if (i < y_sz) diff -= y_digits[i];
        if (diff < 0) {
            diff += 10;
            borrow = 1;
        } else borrow = 0;
        CXP_ASSERT(diff >= 0 && diff < 10);
        res[i] = (cxp_digit_t) diff;
    }
    if (res[res_sz-1] == 0) return res_sz - 1;
    return res_sz;
}

bool cxp_subi(CXP_Ctx *ctx, const CXP_Int *x, const CXP_Int *y, CXP_Int *res)
{
    cxp_size_t max_sz = (x->size > y->size) ? x->size : y->size;
    if (!cxp_initi(ctx, res, max_sz)) return false;

    if (x->sign != y->sign) {
        // either (-x) - y or x - (-y) -> x + y 
        //      = -(x + y) or x + y
        cxp_size_t res_sz = cxp__add_digits(x->digits, x->size, y->digits, y->size, res->digits);
        res->size = res_sz;
        if (x->sign) res->sign = 1; // is negative
        return true;
    }
    const CXP_Int *larger = x;
    const CXP_Int *smaller = y;
    cxp_digit_t res_sign = x->sign;
    int cmp = cxp_abs_cmpi(ctx, x, y);
    if (cmp == 0) {
        return true;
    } else if (cmp < 0) {
        smaller = x;
        larger = y;
        res_sign = 1 - res_sign;
    }
    cxp_size_t res_sz = cxp__sub_digits(larger->digits, larger->size, smaller->digits, smaller->size, res->digits);
    res->size = res_sz;
    res->sign = res_sign;
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
    [CXP_ERR_ROUNDING] = "Rounding error",
    [CXP_ERR_PARSE] = "Parsing error",
};

const char *cxp_strerror(const CXP_Ctx *ctx)
{
    if (*(ctx->err_str)) return ctx->err_str;
    return cxp_error_messages[ctx->err];
}

void cxp_error_reset(CXP_Ctx *ctx)
{
    ctx->err = CXP_OK;
    ctx->err_str[0] = '\0';
}
