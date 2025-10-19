#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "axp.h"


// 
// ------- Helper functions -------

// Some machines apparently has the byte size as something other than 8 bits?
#define AXP_TYPE_MIN_VALUE(T)  (-( (T)1 << (sizeof(T)*CHAR_BIT - 1) ))
#define AXP_TYPE_MAX_VALUE(T)  (  ((T)~AXP_TYPE_MIN_VALUE(T)) )

// This will be needed for multiplication of floats later
static bool axp_safe_add_int64_t(int64_t x, int64_t y, int64_t *res) {
    int64_t min_val = AXP_TYPE_MIN_VALUE(int64_t);
    int64_t max_val = AXP_TYPE_MAX_VALUE(int64_t);

    if ((y > 0 && x > max_val - y) || (y < 0 && x < min_val - y)) return false;

    *res = x + y;
    return true;
}

// This will be needed for exponentiation of floats later
static bool axp_safe_mul_int64_t(int64_t x, int64_t y, int64_t *res) {
    if (x == 0 || y == 0) {
        *res = 0;
        return true;
    }

    int64_t min_val = AXP_TYPE_MIN_VALUE(int64_t);
    int64_t max_val = AXP_TYPE_MAX_VALUE(int64_t);

    if ((x == -1 && y == min_val) || (y == -1 && x == min_val)) {
        return false;
    }

    if (x > 0) {
        if (y > 0 && x > max_val / y) return false;
        if (y < 0 && y < min_val / x) return false;
    } else {
        if (y > 0 && x < min_val / y) return false;
        if (y < 0 && x < max_val / y) return false;
    }

    *res = x * y;
    return true;

}

// ------- Allocation -------

bool axp_initi(AXP_Ctx *ctx, AXP_Int *x, axp_size_t initial_capacity)
{
    x->size = 1;
    x->capacity = initial_capacity;
    x->digits = calloc(initial_capacity, sizeof(axp_digit_t));
    x->sign = 0;
    if (!x->digits) {
        axp_throw(ctx, AXP_ERR_ALLOC, "Memory allocation failed, could not allocate %lu bytes in `axp_initi`.", initial_capacity*sizeof(axp_digit_t));
        return false;
    }
    axp_error_reset(ctx);
    return true;
}

bool axp_initf(AXP_Ctx *ctx, AXP_Float *x)
{
    return axp_initf_ex(ctx, x, ctx->precision);
}

bool axp_initf_ex(AXP_Ctx *ctx, AXP_Float *x, axp_size_t precision)
{
    x->size = 0;
    x->capacity = precision;
    x->digits = calloc(precision, sizeof(axp_digit_t));
    x->sign = 0;
    x->exponent = 0;
    if (!x->digits) {
        axp_throw(ctx, AXP_ERR_ALLOC, "Memory allocation failed, could not allocate %lu bytes in `axp_initf_ex`.", precision*sizeof(axp_digit_t));
        return false;
    }
    axp_error_reset(ctx);
    return true;
}

bool axp_realloci(AXP_Ctx *ctx, AXP_Int *x, axp_size_t size)
{
    if (size < x->size) {
        axp_size_t diff = x->size - size;
        memmove(x->digits, x->digits + diff, size * sizeof(axp_digit_t));
        x->size = size;
    }
    
    axp_digit_t *new_digits = realloc(x->digits, size * sizeof(axp_digit_t));
    if (!new_digits) {
        free(x->digits);
        axp_throw(ctx, AXP_ERR_ALLOC, "Memory allocation failed, could not reallocate %lu bytes in `axp_initf_ex`.", size*sizeof(axp_digit_t));
        return false;
    }
    x->digits = new_digits;
    x->capacity = size;
    return true;
}

bool axp_reallocf(AXP_Ctx *ctx, AXP_Float *x, axp_size_t size)
{
    if (size < x->size) {
        axp_size_t diff = x->size - size;
        memmove(x->digits, x->digits + diff, size * sizeof(axp_digit_t));
        x->size = size;
    }
    
    axp_digit_t *new_digits = realloc(x->digits, size * sizeof(axp_digit_t));
    if (!new_digits) {
        free(x->digits);
        axp_throw(ctx, AXP_ERR_ALLOC, "Memory allocation failed, could not reallocate %lu bytes in `axp_initf_ex`.", size*sizeof(axp_digit_t));
        return false;
    }
    x->digits = new_digits;
    x->capacity = size;
    return true;
}

bool axp_initi_from_str(AXP_Ctx *ctx, const char *str, AXP_Int *x)
{
    if (!str || !*str) {
        axp_throw(ctx, AXP_ERR_PARSE, "Could not parse empty string or null pointer as integer.");
        return false;
    }
    uint8_t sign = 0;
    axp_size_t res_sz = 0;
    axp_size_t allocated = 4;
    axp_digit_t *res_digits = calloc(4, sizeof(axp_digit_t));
    if (!res_digits) {
        axp_throw(ctx, AXP_ERR_ALLOC, "Memory allocation failed, could not allocate %lu bytes in `axp_initi_from_str`.", allocated*sizeof(axp_digit_t));
        return false;
    }

    while(isspace(*str)) str++;

    if (*str == '-') {
        sign = 1;
        str++;
    } else if (*str == '+') str++;

    while (*str) {
        const char chr = *str;
        if (isspace(chr)) {
            str++;
            continue;
        }

        if (!isdigit(chr)) {
            axp_throw(ctx, AXP_ERR_PARSE, "String parsing failed, could not parse '%c' as a digit", chr);
            free(res_digits);
            return false;
        }

        if (res_sz >= allocated) {
            axp_digit_t *tmp = realloc(res_digits, allocated*2*sizeof(axp_digit_t));
            if (!tmp) {
                free(res_digits);
                axp_throw(ctx, AXP_ERR_ALLOC, "Memory allocation failed, could not allocate %lu bytes in `axp_initi_from_str`.", allocated*sizeof(axp_digit_t));
                return false;
            }
            res_digits = tmp;
            allocated *= 2;
        }
        res_digits[res_sz++] = (axp_digit_t)(chr - '0');
        str++;
    }

    // Must be a better way to do this without reversing, but works for now
    for (axp_size_t i = 0; i < res_sz / 2; ++i) {
        axp_digit_t tmp = res_digits[i]; // Digit at index i from the left
        res_digits[i] = res_digits[res_sz - 1 - i]; // Digit from index i from the right
        res_digits[res_sz - 1 - i] = tmp; // Swap
    }
    x->sign = sign;
    x->digits = res_digits;
    x->size = res_sz;
    x->capacity = allocated;
    return true;
}

void axp_freei(AXP_Int *x)
{
    AXP_ASSERT(x->digits);
    free(x->digits);
}

void axp_freef(AXP_Float *x)
{
    AXP_ASSERT(x->digits);
    free(x->digits);
}

bool axp_copyi(AXP_Ctx *ctx, AXP_Int *restrict dst, const AXP_Int *restrict src)
{
    AXP_ASSERT(!dst->digits);
    if (!axp_initi(ctx, dst, dst->capacity)) return false; // Sets the capcity for us
    dst->size = src->size;
    dst->sign = src->sign;
    memcpy(dst->digits, src->digits, src->size*sizeof(axp_digit_t));
    return true; // We do not need to reset error here since init already does and nothing we do after can raise any errors
}

bool axp_copyf(AXP_Ctx *ctx, AXP_Float *restrict dst, const AXP_Float *restrict src)
{
    return axp_copyf_ex(ctx, dst, src, ctx->precision); // We do not need to reset error here since init already does and nothing we do after can raise any errors
}

bool axp_copyf_ex(AXP_Ctx *ctx, AXP_Float *restrict dst, const AXP_Float *restrict src, axp_size_t precision)
{
    AXP_ASSERT(!dst->digits);
    if (!axp_initf_ex(ctx, dst, precision)) return false; // Sets the capacity for us
    dst->sign = src->sign;
    dst->exponent = src->exponent;
    if (src->size > precision) {
        memcpy(dst->digits, src->digits + (src->size - precision), precision * sizeof(axp_digit_t));
        dst->size = precision;
        return true;
    }
    memcpy(dst->digits, src->digits, src->size * sizeof(axp_digit_t));
    dst->size = src->size;
    return true; // We do not need to reset error here since init already does and nothing we do after can raise any errors
}

bool axp_copyf_exact(AXP_Ctx *ctx, AXP_Float *restrict dst, const AXP_Float *restrict src)
{
    AXP_ASSERT(!dst->digits);
    if (!axp_initf_ex(ctx, dst, src->capacity)) return false; // Sets the capacity for us
    dst->sign = src->sign;
    dst->exponent = src->exponent;
    dst->size = src->size;
    memcpy(dst->digits, src->digits, src->size * sizeof(axp_digit_t));
    return true; // We do not need to reset error here since init already does and nothing we do after can raise any errors
}

int axp_abs_cmpi(AXP_Ctx *ctx, const AXP_Int *x, const AXP_Int *y)
{
    // Maybe don't pass ctx to this function, however i like the consistancy.
    (void)ctx;
    if (x->size > y->size) return 1;
    if (x->size < y->size) return -1;
    for (axp_size_t i = x->size; i > 0; i--) {
        if (x->digits[i-1] > y->digits[i-1]) return 1;
        if (x->digits[i-1] < y->digits[i-1]) return -1;
    }
    return 0;
}

axp_size_t axp__add_digits(const axp_digit_t *x_digits, axp_size_t x_sz, const axp_digit_t *y_digits, axp_size_t y_sz, axp_digit_t *res)
{
    axp_digit_t carry = 0;
    axp_size_t res_sz = 0;
    axp_size_t max_sz = (x_sz > y_sz) ? x_sz : y_sz;
    for (axp_size_t i = 0; (i < max_sz) || carry; i++) {
        axp_digit_t sum = carry;
        if (i < x_sz) sum += x_digits[i];
        if (i < y_sz) sum += y_digits[i];
        res[i] = sum % 10;
        carry = sum / 10;
        res_sz++;
    }
    return res_sz;
}

bool axp_addi(AXP_Ctx *ctx, const AXP_Int *x, const AXP_Int *y, AXP_Int *res)
{
    axp_size_t max_sz = (x->size > y->size) ? x->size + 1 : y->size + 1;
    if (!axp_initi(ctx, res, max_sz)) return false;
    if (x->sign != y->sign) {
        if (x->sign) {
            // This is quite repetetive, maybe refactor...
            const AXP_Int *larger = y;
            const AXP_Int *smaller = x;
            axp_digit_t res_sign = 0;
            int cmp = axp_abs_cmpi(ctx, y, x);
            if (cmp == 0) return true;
            else if (cmp < 0) {
                res_sign = 1;
                larger = x;
                smaller = y;
            }
            axp_size_t res_sz = axp__sub_digits(larger->digits, larger->size, smaller->digits, smaller->size, res->digits);
            res->size = res_sz;
            res->sign = res_sign;
            return true;
        } else {
            const AXP_Int *larger = x;
            const AXP_Int *smaller = y;
            axp_digit_t res_sign = 0;
            int cmp = axp_abs_cmpi(ctx, x, y);
            if (cmp == 0) return true;
            else if (cmp < 0) {
                res_sign = 1;
                larger = y;
                smaller = x;
            }
            axp_size_t res_sz = axp__sub_digits(larger->digits, larger->size, smaller->digits, smaller->size, res->digits);
            res->size = res_sz;
            res->sign = res_sign;
            return true;
        }
    }
    
    
    axp_size_t res_sz = axp__add_digits(x->digits, x->size, y->digits, y->size, res->digits);
    res->size = res_sz;
    res->sign = x->sign;
    return true;
}

axp_size_t axp__sub_digits(const axp_digit_t *x_digits, axp_size_t x_sz, const axp_digit_t *y_digits, axp_size_t y_sz, axp_digit_t *res)
{
    axp_digit_t borrow = 0;
    axp_size_t res_sz = x_sz;
    for (axp_size_t i = 0; i < x_sz; i++) {
        int diff = x_digits[i] - borrow;
        if (i < y_sz) diff -= y_digits[i];
        if (diff < 0) {
            diff += 10;
            borrow = 1;
        } else borrow = 0;
        AXP_ASSERT(diff >= 0 && diff < 10);
        res[i] = (axp_digit_t) diff;
    }
    if (res[res_sz-1] == 0) return res_sz - 1;
    return res_sz;
}

bool axp_subi(AXP_Ctx *ctx, const AXP_Int *x, const AXP_Int *y, AXP_Int *res)
{
    axp_size_t max_sz = (x->size > y->size) ? x->size : y->size;
    if (!axp_initi(ctx, res, max_sz)) return false;

    if (x->sign != y->sign) {
        // either (-x) - y or x - (-y) -> x + y 
        //      = -(x + y) or x + y
        axp_size_t res_sz = axp__add_digits(x->digits, x->size, y->digits, y->size, res->digits);
        res->size = res_sz;
        if (x->sign) res->sign = 1; // is negative
        return true;
    }
    const AXP_Int *larger = x;
    const AXP_Int *smaller = y;
    axp_digit_t res_sign = x->sign;
    int cmp = axp_abs_cmpi(ctx, x, y);
    if (cmp == 0) {
        return true;
    } else if (cmp < 0) {
        smaller = x;
        larger = y;
        res_sign = 1 - res_sign;
    }
    axp_size_t res_sz = axp__sub_digits(larger->digits, larger->size, smaller->digits, smaller->size, res->digits);
    res->size = res_sz;
    res->sign = res_sign;
    return false;
}

void axp_throw(AXP_Ctx *ctx, AXP_ErrorCode err_code, const char *fmt, ...) {
    ctx->err = err_code;
    va_list args;
    va_start(args, fmt);
    vsnprintf(ctx->err_str, sizeof(ctx->err_str), fmt, args);
    va_end(args);
}

static const char *axp_error_messages[] = {
    [AXP_OK] = "No error",
    [AXP_ERR_ALLOC] = "Memory allocation failed",
    [AXP_ERR_DIV_ZERO] = "Division by zero",
    [AXP_ERR_OVERFLOW] = "Digit overflow",
    [AXP_ERR_ROUNDING] = "Rounding error",
    [AXP_ERR_PARSE] = "Parsing error",
};

const char *axp_strerror(const AXP_Ctx *ctx)
{
    if (*(ctx->err_str)) return ctx->err_str;
    return axp_error_messages[ctx->err];
}

void axp_error_reset(AXP_Ctx *ctx)
{
    ctx->err = AXP_OK;
    ctx->err_str[0] = '\0';
}
