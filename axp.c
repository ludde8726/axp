#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <errno.h>
#include <unistd.h>

#include "axp.h"


// 
// ------- Helper functions -------

#define AXP_TYPE_MIN_VALUE(T) \
    ( (T)(1ULL << (sizeof(T)*CHAR_BIT - 1)) )

#define AXP_TYPE_MAX_VALUE(T) \
    ( (T)~(T)(1ULL << (sizeof(T)*CHAR_BIT - 1)) )

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
    x->size = 1;
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
        axp_throw(ctx, AXP_ERR_ALLOC, "Memory allocation failed, could not reallocate %lu bytes in `axp_realloci`.", size*sizeof(axp_digit_t));
        return false;
    }
    x->digits = new_digits;
    x->capacity = size;
    axp_error_reset(ctx);
    return true;
}

bool axp_reallocf(AXP_Ctx *ctx, AXP_Float *x, axp_size_t size)
{
    if (size < x->size) {
        axp_size_t diff = x->size - size;
        memmove(x->digits, x->digits + diff, size * sizeof(axp_digit_t));
        x->exponent += diff;
        x->size = size;
    }
    
    axp_digit_t *new_digits = realloc(x->digits, size * sizeof(axp_digit_t));
    if (!new_digits) {
        axp_throw(ctx, AXP_ERR_ALLOC, "Memory allocation failed, could not reallocate %lu bytes in `axp_reallocf`.", size*sizeof(axp_digit_t));
        return false;
    }
    x->digits = new_digits;
    x->capacity = size;
    axp_error_reset(ctx);
    return true;
}

bool axp_reallocf_round(AXP_Ctx *ctx, AXP_Float *x, axp_size_t size) {
    axp_roundf(x, size);
    axp_digit_t *new_digits = realloc(x->digits, size * sizeof(axp_digit_t));
    if (!new_digits) {
        axp_throw(ctx, AXP_ERR_ALLOC, "Memory allocation failed, could not reallocate %lu bytes in `axp_reallocf_round`.", size*sizeof(axp_digit_t));
        return false;
    }
    x->digits = new_digits;
    x->capacity = size;
    axp_error_reset(ctx);
    return true;
}

void axp_printi(const AXP_Int *x) {
    if (x->sign) printf("-");
    for (axp_size_t i = x->size; i > 0; i--) printf("%u", x->digits[i - 1]);
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
    if (!axp_initi(ctx, dst, src->capacity)) return false; // Sets the capcity for us
    dst->size = src->size;
    dst->sign = src->sign;
    memcpy(dst->digits, src->digits, src->size*sizeof(axp_digit_t));
    return true; // We do not need to reset error here since init already does and nothing we do after can raise any errors
}

// TODO: For some reason we dont handle x becoming smaller, can't remember if there's a reason but i just think i forgot
bool axp_copyi_ex(AXP_Ctx *ctx, AXP_Int *restrict dst, const AXP_Int *restrict src, axp_size_t capacity)
{
    AXP_ASSERT(!dst->digits);
    if (!axp_initi(ctx, dst, capacity)) return false; // Sets the capcity for us
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

bool axp_copyf_ex_round(AXP_Ctx *ctx, AXP_Float *restrict dst, const AXP_Float *restrict src, axp_size_t precision) {
    AXP_ASSERT(!dst->digits);
    if (!axp_initf_ex(ctx, dst, precision)) return false; // Sets the capacity for us
    dst->sign = src->sign;
    dst->exponent = src->exponent;

    if (precision < src->size) {
        memcpy(dst->digits, src->digits + (src->size - precision), precision * sizeof(axp_digit_t));
        dst->size = precision;

        axp_size_t diff = src->size - precision;
        if (src->digits[diff - 1] >= 5) {
            axp_digit_t carry = 1;
            for (axp_size_t i = 0; i < precision; i++) {
                axp_digit_t sum = dst->digits[i] + carry;
                dst->digits[i] = sum % 10;
                carry = sum / 10;
                if (!carry) break;
            }
            if (carry) {
                memset(dst->digits, 0, precision * sizeof(axp_digit_t));
                dst->digits[0] = 1;
                dst->size = 1;
                dst->exponent++;
            }
        }
        dst->exponent += diff;
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

int8_t axp__abs_cmpi_digits(axp_digit_t *x_digits, axp_size_t x_sz, axp_digit_t *y_digits, axp_size_t y_sz) {
    if (x_sz > y_sz) return 1;
    if (x_sz < y_sz) return -1;
    for (axp_size_t i = x_sz; i > 0; i--) {
        if (x_digits[i-1] > y_digits[i-1]) return 1;
        if (x_digits[i-1] < y_digits[i-1]) return -1;
    }
    return 0;
}

bool axp_abs_cmpi(AXP_Ctx *ctx, const AXP_Int *x, const AXP_Int *y, int8_t *res) {
    if (!x->digits || !y->digits) {
        axp_throw(ctx, AXP_ERR_UNINITIALIZED, "Cannot compare uninitalized integers.");
        return false;
    }

    *res = axp__abs_cmpi_digits(x->digits, x->size, y->digits, y->size);
    axp_error_reset(ctx);
    return true;
}

bool axp__is_zero_digits(const axp_digit_t *x, axp_size_t x_sz) {
    return (x_sz == 0 || ((x_sz == 1) && (x[0] == 0)));
}

bool axp_is_zeroi(AXP_Ctx *ctx, const AXP_Int *x, bool *res) {
    if (!x->digits) {
        axp_throw(ctx, AXP_ERR_UNINITIALIZED, "Cannot check value of uninitalized integer.");
        return false;
    }
    *res = axp__is_zero_digits(x->digits, x->size);
    axp_error_reset(ctx);
    return true;
}

bool axp_is_zerof(AXP_Ctx *ctx, const AXP_Float *x, bool *res) {
    if (!x->digits) {
        axp_throw(ctx, AXP_ERR_UNINITIALIZED, "Cannot check value of uninitalized float.");
        return false;
    }
    *res = axp__is_zero_digits(x->digits, x->size);
    axp_error_reset(ctx);
    return true;
}


void axp_normalizef(AXP_Float *x) {
    while ((x->size > 1) && (x->digits[x->size-1] == 0)) x->size--;
    axp_size_t needed_shift = 0;

    while ((x->digits[needed_shift] == 0) && (x->size > 1)) needed_shift++;
    if (needed_shift) {
        x->size = axp__shri_digits(x->digits, x->size, needed_shift);
        x->exponent += needed_shift;
    }
}

void axp_roundf(AXP_Float *x, axp_size_t significant_digits) {
   if (significant_digits < x->size) {
        axp_size_t diff = x->size - significant_digits;
        if (x->digits[diff - 1] >= 5) {
            axp_digit_t carry = 1;
            for (axp_size_t i = diff; i < x->size; i++) {
                axp_digit_t sum = x->digits[i] + carry;
                x->digits[i] = sum % 10;
                carry = sum / 10;
                if (!carry) break;
            }
            if (carry) {
                memset(x->digits + diff, 0, significant_digits * sizeof(axp_digit_t));
                x->digits[0] = 1;
                x->size = 1;
                x->exponent++;
            }
        }
        memmove(x->digits, x->digits + diff, significant_digits * sizeof(axp_digit_t));
        x->exponent += diff;
        x->size = significant_digits;
   }
}

axp_size_t axp__shli_digits(axp_digit_t *x_digits, axp_size_t x_sz, axp_size_t shift) {
    if (x_sz == 0 || shift == 0) return x_sz;
    memmove(x_digits + shift, x_digits, x_sz * sizeof(axp_digit_t));
    memset(x_digits, 0, shift * sizeof(axp_digit_t));
    return x_sz + shift;
}

void axp_shli(AXP_Ctx *ctx, AXP_Int *x, axp_size_t shift) {
    (void) ctx;
    if (x->size == 0 || shift == 0) return;
    axp_size_t new_size = x->size + shift;

    if (new_size > x->capacity) {
        if (x->capacity <= shift) {
            memset(x->digits, 0, x->capacity * sizeof(axp_digit_t));
            return;
        }
        axp_size_t keep = x->capacity - shift;
        memmove(x->digits + shift, x->digits + (x->size - keep), keep * sizeof(axp_digit_t));
        memset(x->digits, 0, shift * sizeof(axp_digit_t));
        x->size = x->capacity;
        return;
    }
    memmove(x->digits + shift, x->digits, x->size * sizeof(axp_digit_t));
    memset(x->digits, 0, shift * sizeof(axp_digit_t));
    x->size = new_size;
}

axp_size_t axp__shri_digits(axp_digit_t *x_digits, axp_size_t x_sz, axp_size_t shift) {
    if (x_sz == 0 || shift == 0) return x_sz;

    if (shift >= x_sz) {
        memset(x_digits, 0, x_sz * sizeof(axp_digit_t));
        return 1;
    }
    memmove(x_digits, x_digits + shift, (x_sz - shift) * sizeof(axp_digit_t));
    memset(x_digits + (x_sz - shift), 0, shift * sizeof(axp_digit_t));
    return x_sz - shift;
}

void axp_shri(AXP_Ctx *ctx, AXP_Int *x, axp_size_t shift) {
    (void) ctx;
    if (x->size == 0 || shift == 0) return;

    if (shift >= x->size) {
        x->size = 1;
        memset(x->digits, 0, x->capacity * sizeof(axp_digit_t));
        return;
    }
    memmove(x->digits, x->digits + shift, (x->size - shift) * sizeof(axp_digit_t));
    memset(x->digits + (x->size - shift), 0, shift * sizeof(axp_digit_t));
    x->size -= shift;
}

void axp_align_float_digits(AXP_Float *x, AXP_Float *y) {
    if (x->exponent == y->exponent) return;

    if (x->exponent < y->exponent) {
        AXP_Float *higher = y;
        y = x;
        x = higher;
    }

    axp_size_t needed_shift = (axp_size_t)(x->exponent - y->exponent);
    axp_size_t available_left_space = x->capacity - x->size;
    axp_size_t left_shift = (needed_shift > available_left_space) ? available_left_space : needed_shift;
    x->size = axp__shli_digits(x->digits, x->size, left_shift);
    x->exponent -= left_shift;
    needed_shift -= left_shift;
    if (needed_shift > 0) {
        y->size = axp__shri_digits(y->digits, y->size, needed_shift);
        y->exponent += needed_shift;
    }
}

axp_size_t axp__add_digits(const axp_digit_t *x_digits, axp_size_t x_sz, const axp_digit_t *y_digits, axp_size_t y_sz, axp_digit_t *res)
{
    axp_digit_t carry = 0;
    axp_size_t res_sz = 0;
    axp_size_t max_sz = (x_sz > y_sz) ? x_sz : y_sz;
    for (axp_size_t i = 0; (i < max_sz) || carry; i++) {
        axp_digit_t sum = carry;
        AXP_ASSERT(i <= max_sz);
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
        const AXP_Int *larger = x;
        const AXP_Int *smaller = y;
        axp_digit_t res_sign = 0;

        if (x->sign) {
            larger = y;
            smaller = x;
        }
        int8_t cmp;
        if (!axp_abs_cmpi(ctx, larger, smaller, &cmp)) {
            axp_freei(res);
            return false;
        }
        if (cmp == 0) return true;
        else if (cmp < 0) {
            const AXP_Int *tmp = larger;
            larger = smaller;
            smaller = tmp;
            res_sign = 1;
        }
        axp_size_t res_sz = axp__sub_digits(larger->digits, larger->size, smaller->digits, smaller->size, res->digits);
        res->size = res_sz;
        res->sign = res_sign;
        return true;
    }
    
    axp_size_t res_sz = axp__add_digits(x->digits, x->size, y->digits, y->size, res->digits);
    res->size = res_sz;
    res->sign = x->sign;
    return true;
}

bool axp_addf(AXP_Ctx *ctx, const AXP_Float *x, const AXP_Float *y, AXP_Float *res) {
    if (!axp_initf_ex(ctx, res, ctx->precision + 1)) return false;
    AXP_Float x_cpy = { 0 };
    AXP_Float y_cpy = { 0 };
    if (!axp_copyf_ex_round(ctx, &x_cpy, x, ctx->precision)) goto cleanup_error;
    if (!axp_copyf_ex_round(ctx, &y_cpy, y, ctx->precision)) goto cleanup_error;

    axp_align_float_digits(&x_cpy, &y_cpy);

    if (x_cpy.sign != y_cpy.sign) {
        AXP_Float larger = x_cpy;
        AXP_Float smaller = y_cpy;
        uint8_t res_sign = 0;

        if (x_cpy.sign) {
            larger = y_cpy;
            smaller = x_cpy;
        }

        int8_t cmp = axp__abs_cmpi_digits(larger.digits, larger.size, smaller.digits, smaller.size);

        if (cmp == 0) goto cleanup_success;
        else if (cmp < 0) {
            AXP_Float tmp = larger;
            larger = smaller;
            smaller = tmp;
            res_sign = 1;
        }
        axp_size_t res_sz = axp__sub_digits(larger.digits, larger.size, smaller.digits, smaller.size, res->digits);
        res->size = res_sz;
        res->sign = res_sign;
        res->exponent = x_cpy.exponent;
        axp_normalizef(res);
        
        if (!axp_reallocf_round(ctx, res, ctx->precision)) goto cleanup_error;
        goto cleanup_success;
    }

    axp_size_t res_sz = axp__add_digits(x_cpy.digits, x_cpy.size, y_cpy.digits, y_cpy.size, res->digits);
    res->size = res_sz;
    res->exponent = x_cpy.exponent;
    res->sign = x_cpy.sign;
    axp_normalizef(res);
    if (!axp_reallocf_round(ctx, res, ctx->precision)) goto cleanup_error;

cleanup_success:
    axp_freef(&x_cpy);
    axp_freef(&y_cpy);
    return true;
cleanup_error:
    axp_freef(res);
    axp_freef(&x_cpy);
    axp_freef(&y_cpy);
    return false;
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
    while (res_sz && res[res_sz-1] == 0) res_sz--;
    return res_sz;
}

bool axp_subi(AXP_Ctx *ctx, const AXP_Int *x, const AXP_Int *y, AXP_Int *res)
{
    axp_size_t max_sz = ((x->size > y->size) ? x->size : y->size) + 1; // + 1 to handle the case for x - (-y) where the addition overflows the buffer
    if (!axp_initi(ctx, res, max_sz)) return false;

    if (x->sign != y->sign) {
        // either (-x) - y or x - (-y)
        //      = -(x + y) or x + y
        axp_size_t res_sz = axp__add_digits(x->digits, x->size, y->digits, y->size, res->digits);
        res->size = res_sz;
        res->sign = x->sign; // is negative
        return true;
    }
    const AXP_Int *larger = x;
    const AXP_Int *smaller = y;
    axp_digit_t res_sign = x->sign;
    int8_t cmp;
    if (!axp_abs_cmpi(ctx, x, y, &cmp)) {
        axp_freei(res);
        return false;
    }
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
    return true;
}

bool axp_subf(AXP_Ctx *ctx, const AXP_Float *x, const AXP_Float *y, AXP_Float *res) {
    if (!axp_initf_ex(ctx, res, ctx->precision + 1)) return false;
    AXP_Float x_cpy = { 0 };
    AXP_Float y_cpy = { 0 };
    if (!axp_copyf_ex_round(ctx, &x_cpy, x, ctx->precision)) goto cleanup_error;
    if (!axp_copyf_ex_round(ctx, &y_cpy, y, ctx->precision)) goto cleanup_error;

    axp_align_float_digits(&x_cpy, &y_cpy);

    if (x_cpy.sign != y_cpy.sign) {
        axp_size_t res_sz = axp__add_digits(x_cpy.digits, x_cpy.size, y_cpy.digits, y_cpy.size, res->digits);
        res->exponent = x_cpy.exponent;
        res->size = res_sz;
        res->sign = x_cpy.sign;
        res->exponent = x_cpy.exponent;
        axp_normalizef(res);
        if (!axp_reallocf_round(ctx, res, ctx->precision)) goto cleanup_error; 
        goto cleanup_success;
    }

    uint8_t res_sign = x_cpy.sign;

    int8_t cmp = axp__abs_cmpi_digits(x_cpy.digits, x_cpy.size, y_cpy.digits, y_cpy.size);

    AXP_Float larger = x_cpy;
    AXP_Float smaller = y_cpy;

    if (cmp == 0) goto cleanup_success;
    else if (cmp < 0) {
        larger = y_cpy;
        smaller = x_cpy;
        res_sign = 1 - res_sign; // Flip between 0 and 1
    }

    axp_size_t res_sz = axp__sub_digits(larger.digits, larger.size, smaller.digits, smaller.size, res->digits);
    res->size = res_sz;
    res->sign = res_sign;
    res->exponent = x_cpy.exponent;
    axp_normalizef(res);
    
    if (!axp_reallocf_round(ctx, res, ctx->precision)) goto cleanup_error;

cleanup_success:
    axp_freef(&x_cpy);
    axp_freef(&y_cpy);
    return true;

cleanup_error:
    axp_freef(res);
    axp_freef(&x_cpy);
    axp_freef(&y_cpy);
    return false;
}

axp_size_t axp__mul_digits(const axp_digit_t *x_digits, axp_size_t x_sz, const axp_digit_t *y_digits, axp_size_t y_sz, axp_digit_t *res) {
    axp_size_t i;
    axp_size_t j;
    axp_size_t max_written = 0;
    for (i = 0; i < x_sz; i++) {
        axp_digit_t carry = 0;
        for (j = 0; j < y_sz || carry; j++) {
            axp_digit_t product = res[i + j] + x_digits[i] * (j < y_sz ? y_digits[j] : 0) + carry;
            res[i + j] = product % BASE;
            carry = product / BASE;

            if (i + j > max_written) max_written = i + j;
        }
    }
    return max_written + 1; // Result size is always the largest accessed index in res
}

bool axp_muli(AXP_Ctx *ctx, const AXP_Int *x, const AXP_Int *y, AXP_Int *res) {
    bool x_zero, y_zero;
    if (!(axp_is_zeroi(ctx, x, &x_zero) && axp_is_zeroi(ctx, y, &y_zero))) return false;
    
    if (x_zero || y_zero) {
        if (!axp_initi(ctx, res, 1)) return false;
        res->size = 1;
        return true;
    }

    if (!axp_initi(ctx, res, x->size + y->size)) return false;
    
    axp_size_t res_sz = axp__mul_digits(x->digits, x->size, y->digits, y->size, res->digits);
    res->size = res_sz;
    res->sign = x->sign ^ y->sign;
    return true;
}

bool axp_mulf(AXP_Ctx *ctx, const AXP_Float *x, const AXP_Float *y, AXP_Float *res) {
    bool x_zero, y_zero;
    if (!(axp_is_zerof(ctx, x, &x_zero) && axp_is_zerof(ctx, y, &y_zero))) return false;
    if (x_zero || y_zero) {
        if (!axp_initf_ex(ctx, res, ctx->precision)) return false;
        res->size = 1;
        return true;
    }

    if (!axp_initf_ex(ctx, res, ctx->precision * 2 + 2)) return false;
    AXP_Float x_cpy = { 0 };
    AXP_Float y_cpy = { 0 };
    if (!axp_copyf_ex_round(ctx, &x_cpy, x, ctx->precision + 1)) goto cleanup_error;
    if (!axp_copyf_ex_round(ctx, &y_cpy, y, ctx->precision + 1)) goto cleanup_error;

    axp_size_t res_sz = axp__mul_digits(x_cpy.digits, x_cpy.size, y_cpy.digits, y_cpy.size, res->digits);
    res->size = res_sz;
    res->sign = x->sign ^ y->sign;
    // Exponent overflow check
    if ((y->exponent > 0 && x->exponent > AXP_TYPE_MAX_VALUE(axp_exp_t) - y->exponent) ||
        (y->exponent < 0 && x->exponent < AXP_TYPE_MIN_VALUE(axp_exp_t) - y->exponent)) {
        axp_throw(ctx, AXP_ERR_OVERFLOW, "Exponent overflow (%lld + %lld)", x->exponent, y->exponent);
        goto cleanup_error;
    }

    res->exponent = x->exponent + y->exponent;
    axp_normalizef(res);
    if (!axp_reallocf_round(ctx, res, ctx->precision)) goto cleanup_error;
    axp_freef(&x_cpy);
    axp_freef(&y_cpy);
    axp_error_reset(ctx);
    return true;

cleanup_error:
    axp_freef(res);
    axp_freef(&x_cpy);
    axp_freef(&y_cpy);
    return false;
}

axp_size_t axp__div_digits(axp_digit_t *x_digits, axp_size_t x_sz, axp_digit_t *y_digits, axp_size_t y_sz, axp_digit_t *res, axp_size_t *remainder_sz) {
    axp_size_t shift = x_sz - y_sz;
    axp_size_t shifted_sz = axp__shli_digits(y_digits, y_sz, shift);
    *remainder_sz = x_sz;
    axp_size_t res_sz = 0;
    bool first_access = false;

    for (int64_t i = shift; i >= 0; i--) {
        axp_size_t count = 0;
        while (axp__abs_cmpi_digits(x_digits, *remainder_sz, y_digits, shifted_sz) >= 0) {
            *remainder_sz = axp__sub_digits(x_digits, *remainder_sz, y_digits, shifted_sz, x_digits);
            count++;
        }
        res[i] = (axp_digit_t) count;
        if (first_access || (count != 0)) {
            first_access = true;
            res_sz++;
        }
        shifted_sz = axp__shri_digits(y_digits, shifted_sz, 1);
    }
    return res_sz;
}

axp_size_t axp__div_digits_float(axp_digit_t *x_digits, axp_size_t x_sz, axp_digit_t *y_digits, axp_size_t y_sz, axp_digit_t *res, axp_size_t max_prec, axp_exp_t *exp_adjust) {
    axp_size_t res_sz = 0;
    
    if (x_sz >= y_sz) {
        axp_size_t shift = x_sz - y_sz;
        y_sz = axp__shli_digits(y_digits, y_sz, shift);
        *exp_adjust = (axp_exp_t)shift;
    } else {
        axp_size_t shift = y_sz - x_sz;
        x_sz = axp__shli_digits(x_digits, x_sz, shift);
        *exp_adjust = -(axp_exp_t)shift;
    }
    bool first_access = false;

    while (res_sz < max_prec) {
        if (axp__is_zero_digits(x_digits, x_sz)) {
            memmove(res, res + (max_prec - res_sz), res_sz * sizeof(axp_digit_t));
            break;
        }
        axp_size_t count = 0;

        while (axp__abs_cmpi_digits(x_digits, x_sz, y_digits, y_sz) >= 0) {
            x_sz = axp__sub_digits(x_digits, x_sz, y_digits, y_sz, x_digits);
            count++;
        }

        res[max_prec - res_sz - 1] = (axp_digit_t) count;
        if (first_access || (count != 0)) {
            first_access = true;
            res_sz++;
        }
        (*exp_adjust)--;
        x_sz = axp__shli_digits(x_digits, x_sz, 1);
    }
    (*exp_adjust)++; // Don't know why but this works...
    return res_sz;
}

bool axp_divi(AXP_Ctx *ctx, const AXP_Int *x, const AXP_Int *y, AXP_Int *res, AXP_Int *remainder) {
    bool is_zero;
    if (!axp_is_zeroi(ctx, y, &is_zero)) return false;
    if (is_zero) {
        axp_throw(ctx, AXP_ERR_DIV_ZERO, "Division by zero in `axp_divi`");
        return false;
    }

    AXP_Int y_copy = { 0 };
    AXP_Int x_copy = { 0 };
    axp_size_t remainder_sz = x->size;
    if (!axp_copyi_ex(ctx, &y_copy, y, y->size > x->size ? y->size : x->size)) goto cleanup_error;
    if (!axp_copyi(ctx, &x_copy, x)) goto cleanup_error;

    if (!axp_initi(ctx, res, x->size)) goto cleanup_error;

    int8_t cmp;
    if (!axp_abs_cmpi(ctx, x, y, &cmp)) goto cleanup_error;
    if (cmp == -1) goto cleanup_success;

    res->size = axp__div_digits(x_copy.digits, x_copy.size, y_copy.digits, y_copy.size, res->digits, &remainder_sz);
    goto cleanup_success;
cleanup_error:
    axp_freei(&x_copy);
    axp_freei(&y_copy);
    axp_freei(res);
    return false;
cleanup_success:
    // a = bq + r => a/b = q + r/b
    res->sign = x->sign ^ y->sign;
    if (remainder) {
        *remainder = x_copy;
        remainder->size = remainder_sz ? remainder_sz : 1;
        remainder->sign = x->sign;
    }
    else axp_freei(&x_copy);
    axp_freei(&y_copy);
    axp_error_reset(ctx);
    return true;
}

bool axp_divf(AXP_Ctx *ctx, const AXP_Float *x, const AXP_Float *y, AXP_Float *res) {
    bool x_zero, y_zero;
    if (!(axp_is_zerof(ctx, x, &x_zero) && axp_is_zerof(ctx, y, &y_zero))) return false;
    if (x_zero) {
        if (!axp_initf_ex(ctx, res, ctx->precision)) return false;
        res->size = 1;
        return true;
    } else if (y_zero) {
        axp_throw(ctx, AXP_ERR_DIV_ZERO, "Division by zero in `axp_divf`");
        return false;
    }

    if (!axp_initf_ex(ctx, res, ctx->precision + 1)) return false;
    AXP_Float x_cpy = { 0 };
    AXP_Float y_cpy = { 0 };
    if (!axp_copyf_ex_round(ctx, &x_cpy, x, ctx->precision + 1)) goto cleanup_error;
    if (!axp_copyf_ex_round(ctx, &y_cpy, y, ctx->precision + 1)) goto cleanup_error;

    axp_exp_t exp_adjust;

    axp_size_t res_sz = axp__div_digits_float(x_cpy.digits, x_cpy.size, y_cpy.digits, y_cpy.size, res->digits, ctx->precision + 1, &exp_adjust);
    res->size = res_sz;
    res->sign = x->sign ^ y->sign;
    // Exponent overflow check
    if ((y->exponent < 0 && x->exponent > AXP_TYPE_MAX_VALUE(axp_exp_t) + y->exponent) ||
        (y->exponent > 0 && x->exponent < AXP_TYPE_MIN_VALUE(axp_exp_t) + y->exponent)) {
        axp_throw(ctx, AXP_ERR_OVERFLOW, "Exponent overflow (%lld - %lld)", x->exponent, y->exponent);
        goto cleanup_error;
    }
    res->exponent = x->exponent - y->exponent;

    if ((exp_adjust > 0 && res->exponent > AXP_TYPE_MAX_VALUE(axp_exp_t) - exp_adjust) ||
        (exp_adjust < 0 && res->exponent < AXP_TYPE_MIN_VALUE(axp_exp_t) - exp_adjust)) {
        axp_throw(ctx, AXP_ERR_OVERFLOW, "Exponent overflow (%lld + %lld)", res->exponent, exp_adjust);
        goto cleanup_error;
    }

    res->exponent += exp_adjust;
    axp_normalizef(res);
    if (!axp_reallocf_round(ctx, res, ctx->precision)) goto cleanup_error;
    axp_freef(&x_cpy);
    axp_freef(&y_cpy);
    axp_error_reset(ctx);
    return true;

cleanup_error:
    axp_freef(res);
    axp_freef(&x_cpy);
    axp_freef(&y_cpy);
    return false;
    return false;
}

// Calculate the res size (max) by y * x->size (always between y * (x->size - 1) + 1 and y * x->size)
axp_size_t axp__pow_digits(axp_digit_t *x_digits, axp_size_t x_sz, axp_size_t y, axp_digit_t *tmp_buf, axp_digit_t *res) {
    res[0] = 1;
    axp_size_t res_sz = 1;
    axp_size_t remainder = 0;
    while (y != 0) {
        remainder = y % 2;
        y /= 2;

        if (remainder) {
            res_sz = axp__mul_digits(res, res_sz, x_digits, x_sz, tmp_buf);
            memcpy(res, tmp_buf, res_sz * sizeof(axp_digit_t));
            memset(tmp_buf, 0, res_sz * sizeof(axp_digit_t));
        }
        if (y != 0) {
            x_sz = axp__mul_digits(x_digits, x_sz, x_digits, x_sz, tmp_buf);
            memcpy(x_digits, tmp_buf, x_sz * sizeof(axp_digit_t));
            memset(tmp_buf, 0, x_sz * sizeof(axp_digit_t));
        }
    }
    return res_sz;
}

bool axp_powi(AXP_Ctx *ctx, AXP_Int *x, axp_size_t y, AXP_Int *res) {
    bool is_zero;
    if (!axp_is_zeroi(ctx, x, &is_zero)) return false;
    if (is_zero && !(y == 0)) {
        if (!axp_copyi(ctx, res, x)) return false;
    } else if (is_zero && y == 0) {
        axp_throw(ctx, AXP_ERR_DIV_ZERO, "0^0 is undefined.");
        return false;
    }

    if (y == 0) {
        axp_initi(ctx, res, 1);
        res->digits[0] = 1;
        return true;
    }

    double logx = floor((y * (x->size - 1 + log10(x->digits[x->size - 1] + 1))));
    
    if (logx > (double)((axp_size_t)-1)) {
        axp_throw(ctx, AXP_ERR_OVERFLOW, "Integer overflow.");
        return false;
    }
    
    axp_size_t max_sz = (axp_size_t)logx + 1;
    
    AXP_Int tmp_buf;
    AXP_Int x_copy = { 0 };
    if (!axp_initi(ctx, res, max_sz)) goto cleanup_error;
    if (!axp_initi(ctx, &tmp_buf, max_sz)) goto cleanup_error;
    if (!axp_copyi_ex(ctx, &x_copy, x, max_sz)) goto cleanup_error;

    res->size = axp__pow_digits(x_copy.digits, x_copy.size, y, tmp_buf.digits, res->digits);
    res->sign = (y % 2) && x->sign;
    axp_freei(&tmp_buf);
    axp_freei(&x_copy);
    axp_error_reset(ctx);
    return true;

cleanup_error:
    axp_freei(res);
    axp_freei(&tmp_buf);
    axp_freei(&x_copy);
    return false;
}

size_t axp_itoa(AXP_Int *x, char *buf, size_t buf_sz) {
    bool should_write = !(buf == NULL || buf_sz == 0);
    size_t needed_space = x->size ? x->size + 1 : 2;
    if (x->sign) needed_space++;
    if (!should_write) return needed_space;

    if (x->sign) *buf++ = '-';
    for (axp_size_t i = x->size; i > 0; i--) {
        *buf++ = '0' + x->digits[i-1];
    }
    *buf = '\0';
    return needed_space;
}

char *axp_itoa_alloc(AXP_Ctx *ctx, AXP_Int *x) {
    size_t needed_space = axp_itoa(x, NULL, 0);
    char *buf = malloc(needed_space * sizeof(char));
    if (!buf) {
        axp_throw(ctx, AXP_ERR_ALLOC, "Memory allocation failed, could not allocate %lu bytes in `axp_itoa_alloc`.", needed_space*sizeof(char));
        return NULL;
    }
    axp_itoa(x, buf, needed_space);
    axp_error_reset(ctx);
    return buf;
}

bool axp_atoi(AXP_Ctx *ctx, const char *str, AXP_Int *x)
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
        axp_throw(ctx, AXP_ERR_ALLOC, "Memory allocation failed, could not allocate %lu bytes in `atoi`.", allocated*sizeof(axp_digit_t));
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
                axp_throw(ctx, AXP_ERR_ALLOC, "Memory allocation failed, could not allocate %lu bytes in `axp_atoi`.", allocated*sizeof(axp_digit_t));
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
    axp_error_reset(ctx);
    return true;
}

size_t axp_ftoa(AXP_Float *x, char *buf, size_t buf_sz) {
    bool should_write = !(buf == NULL || buf_sz == 0);
    size_t needed_space = 0;

    if (x->size == 0 || ((x->size == 1) && (x->digits[0] == 0))) {
        if (should_write) snprintf(buf, 4, "0.0");
        needed_space = 4;
        goto return_block;
    } else if (x->exponent >= 0) {
        needed_space = (size_t)x->exponent + 3; // space for .0\0
        if (x->sign) needed_space++;
        needed_space += x->size;
        if (!should_write) return needed_space;
        if (x->sign) *buf++ = '-';

        for (axp_size_t i = x->size; i > 0; i--) {
            *buf++ = '0' + x->digits[i-1];
        }
        memset(buf, '0', (size_t)x->exponent * sizeof(char));
        buf += x->exponent;
        *buf++ = '.';
        *buf++ = '0';
        *buf = '\0';
        goto return_block;

    } else if (x->size + x->exponent <= 0) {
        size_t leading_zeroes = (size_t)llabs(x->size + x->exponent);
        needed_space = leading_zeroes + 3; // 0. ... \0
        if (x->sign) needed_space++;
        needed_space += x->size;

        if (!should_write) return needed_space;
        if (x->sign) *buf++ = '-';
        *buf++ = '0';
        *buf++ = '.';

        for (size_t i = 0; i < leading_zeroes; i++) *buf++ = '0';
        for (axp_size_t i = x->size; i > 0; i--) *buf++ = '0' + x->digits[i-1];
        *buf = '\0';
        goto return_block;

    } else {
        needed_space = 2; // ... . ... \0
        if (x->sign) needed_space++;
        needed_space += x->size;

        if (!should_write) return needed_space;

        axp_size_t decimal_index = (axp_size_t)-x->exponent;

        for (axp_size_t i = x->size; i > 0; i--) {
            if (i == decimal_index) *buf++ = '.';
            *buf++ = '0' + x->digits[i-1];
        }
        *buf = '\0';
        goto return_block;
    }
return_block:
    return needed_space;
}

char *axp_ftoa_alloc(AXP_Ctx *ctx, AXP_Float *x) {
    size_t needed_space = axp_ftoa(x, NULL, 0);
    char *buf = malloc(needed_space * sizeof(char));
    if (!buf) {
        axp_throw(ctx, AXP_ERR_ALLOC, "Memory allocation failed, could not allocate %lu bytes in `axp_ftoa_alloc`.", needed_space*sizeof(char));
        return NULL;
    }
    axp_ftoa(x, buf, needed_space);
    axp_error_reset(ctx);
    return buf;
}

// TODO: Parsing just '0' fails.
bool axp_atof(AXP_Ctx *ctx, const char *str, AXP_Float *x) {
    if (!str || !*str) {
        axp_throw(ctx, AXP_ERR_PARSE, "Could not parse empty string or null pointer as float.");
        return false;
    }

    if (!axp_initf(ctx, x)) return false;

    axp_size_t precision = x->capacity;

    while (isspace(*str)) str++;

    uint8_t sign = 0;
    if (*str == '-') { 
        sign = 1; 
        str++; 
    } else if (*str == '+') str++;

    const char *scan = str;
    const char *start = NULL;
    bool leading = true;
    bool truncated = false;


    int64_t digit_count = 0;
    int64_t dot_pos = -1;
    int64_t zeroes_after_dot = 0;

    axp_size_t res_sz = 0;
    axp_digit_t first_dropped = 0;

    while (*scan && *scan != 'e' && *scan != 'E') {
        if (*scan == '.') {
            if (dot_pos != -1) {
                axp_throw(ctx, AXP_ERR_PARSE, "Multiple decimal points in float string.");
                axp_freef(x);
                return false;
            }
            if (!start) start = scan;
            dot_pos = digit_count;
        } else if (!start) {
            if (*scan == '0') scan++;
            else if(isdigit(*scan)) {
                start = scan;
                digit_count++;
                scan++;
            } else if (isspace(*scan)) scan++;
            else {
                axp_throw(ctx, AXP_ERR_PARSE, "String parsing failed, could not parse '%c' as a digit", *scan);
                axp_freef(x);
                return false;
            }
            continue;
        } else if (isdigit(*scan)) digit_count++;
        else if (!isspace(*scan)) {
            axp_throw(ctx, AXP_ERR_PARSE, "String parsing failed, could not parse '%c' as a digit", *scan);
            axp_freef(x);
            return false;
        }
        scan++;
    }
    if (digit_count == 0) {
        axp_throw(ctx, AXP_ERR_PARSE, "No digits found in float string.");
        axp_freef(x);
        return false;
    }
    if (dot_pos == -1) dot_pos = digit_count;

    axp_exp_t str_exp = 0;
    if (*scan == 'e' || *scan == 'E') {
        scan++;
        int8_t exp_sign = 1;
        if (*scan == '-') { 
            exp_sign = -1; 
            scan++; 
        } else if (*scan == '+') scan++;

        if (!isdigit(*scan)) {
            axp_throw(ctx, AXP_ERR_PARSE, "Expected exponent digits after 'e'/'E'.");
            axp_freef(x);
            return false;
        }
        while (isdigit(*scan)) str_exp = str_exp * 10 + (*scan++ - '0');
        str_exp *= exp_sign;
    }

    while (*start && *start != 'e' && *start != 'E') {
        const char chr = *start++;

        if (isspace(chr)) continue;
        if (chr == '.')  continue;
        if (!isdigit(chr)) {
            axp_throw(ctx, AXP_ERR_PARSE, "String parsing failed, could not parse '%c' as a digit", chr);
            axp_freef(x);
            return false;
        }
        axp_digit_t d = (axp_digit_t)(chr - '0');
        if (leading && d == 0) {
            zeroes_after_dot += 1;
            continue;
        }
        leading = false;

        if (res_sz < precision) {
            x->digits[precision - res_sz - 1] = d;
            res_sz++;
        } else {
            if (!truncated) {
                first_dropped = d;
                truncated = true;
            }
        }
    }

    if (res_sz < precision) memmove(x->digits, x->digits + (precision - res_sz), res_sz * sizeof(axp_digit_t));

    if (truncated && first_dropped >= 5 && res_sz > 0) {
        axp_size_t i = 0;
        axp_digit_t carry = 1;
        while (i < res_sz) {
            axp_digit_t sum = x->digits[i] + carry;
            x->digits[i] = sum % 10;
            carry = sum / 10;
            if (!carry) break;
            i++;
        }
        if (carry) {
            memset(x->digits, 0, precision * sizeof(axp_digit_t));
            x->digits[0] = 1;
            dot_pos++;
            res_sz = 1;
        }
    }

    x->sign = sign;
    x->size = res_sz;
    x->exponent = -digit_count + dot_pos + str_exp;
    if (truncated) x->exponent += (digit_count - res_sz) - zeroes_after_dot;
    axp_error_reset(ctx);
    return true;
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
    [AXP_ERR_UNINITIALIZED] = "Uninitialized number error",
    [AXP_ERR_FORMAT] = "Format error",
    [AXP_ERR_WRITE] = "Writing error",
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

int axp__printf_core(AXP_Ctx *ctx, axp__print_target *target, const char *fmt, va_list *args) {
    if (target->type == AXP__TARGET_BUF) {
        if (!(target->as.buf && target->as.buf_sz)) {
            target->as.buf = NULL;
            target->as.buf_sz = 0;
        }
    } else if (target->type == AXP__TARGET_FD) {
        if (!target->as.fd) {
            axp_throw(ctx, AXP_ERR_WRITE, "Cannot write to null stream");
            return -1;
        }
    } else UNREACHABLE("target type");

    int out_pos = 0;
    bool write_failed = false;

    #define _WRITE_STR(src, len) do {                                                       \
        int written = target->write_fn(ctx, &target->as, (src), (size_t)(len), &out_pos);   \
        if (written < 0) write_failed = true;                                               \
        else write_failed = false;                                                          \
    } while (0)

    #define _WRITE_FMT_SPEC(...) do {                                                                       \
        int _needed_buf_len = (int)snprintf(NULL, 0, spec_fmt_str, __VA_ARGS__);                            \
        if (_needed_buf_len < 0) {                                                                          \
            axp_throw(ctx, AXP_ERR_PARSE,                                                                   \
                           "Could not parse format string %s.",                                             \
                           spec_fmt_str                                                                     \
            );                                                                                              \
            return -1;                                                                                      \
        }                                                                                                   \
        char *_tmp_buf = malloc((size_t)(_needed_buf_len + 1) * sizeof(char));                              \
        if (!_tmp_buf) {                                                                                    \
            axp_throw(ctx, AXP_ERR_ALLOC,                                                                   \
                           "Memory allocation failed, could not allocate %lu bytes in `axp__printf_core`.", \
                           (uint32_t)_needed_buf_len*sizeof(char));                                         \
            return -1;                                                                                      \
        }                                                                                                   \
        int _actually_written = snprintf(_tmp_buf, (size_t)_needed_buf_len + 1, spec_fmt_str, __VA_ARGS__); \
        AXP_ASSERT(_actually_written == _needed_buf_len);                                                   \
        _WRITE_STR(_tmp_buf, _needed_buf_len);                                                            \
        free(_tmp_buf);                                                                                     \
        if (write_failed) return -1;                                                                        \
    } while (0)

    #define _RENDER_FMT_SPEC(type) do {                              \
        if (has_star_width && has_star_prec) {                          \
            type _argument = va_arg(*args, type);                       \
            _WRITE_FMT_SPEC(star_width, star_prec, _argument);          \
        } else if (has_star_width) {                                    \
            type _argument = va_arg(*args, type);                       \
            _WRITE_FMT_SPEC(star_width, _argument);                     \
        } else if (has_star_prec) {                                     \
            type _argument = va_arg(*args, type);                       \
            _WRITE_FMT_SPEC(star_prec, _argument);                      \
        } else {                                                        \
            type _argument = va_arg(*args, type);                       \
            _WRITE_FMT_SPEC(_argument);                                 \
        }                                                               \
    } while (0)
    const char *chr = fmt;

    while (*chr) {
        if (*chr != '%') {
            const char *txt_start = chr;
            while (*chr && *chr != '%') chr++;
            _WRITE_STR(txt_start, chr - txt_start);
            if (write_failed) return -1;
            continue;
        }

        const char *fmt_start = chr;
        chr++;

        if (*chr == '%') {
            _WRITE_STR(chr, 1);
            if (write_failed) return -1;
            chr++;

            continue;
        }

        if (*chr == 'Z') {
            AXP_Int arg = va_arg(*args, AXP_Int);
            size_t needed_space = axp_itoa(&arg, NULL, 0);
            char *tmp_buf = malloc(needed_space * sizeof(char));
            if (!tmp_buf) {
                axp_throw(ctx, AXP_ERR_ALLOC, "Memory allocation failed, could not allocate %lu bytes in `axp__printf_core`.", needed_space*sizeof(char));
                return -1;
            }
            axp_itoa(&arg, tmp_buf, needed_space);
            _WRITE_STR(tmp_buf, needed_space - 1);
            free(tmp_buf);
            if (write_failed) return -1;
            chr++;
            continue;
        } else if (*chr == 'R') {
            AXP_Float arg = va_arg(*args, AXP_Float);
            size_t needed_space = axp_ftoa(&arg, NULL, 0);
            char *tmp_buf = malloc(needed_space * sizeof(char));
            if (!tmp_buf) {
                axp_throw(ctx, AXP_ERR_ALLOC, "Memory allocation failed, could not allocate %lu bytes in `axp__printf_core`.", needed_space*sizeof(char));
                return -1;
            }
            axp_ftoa(&arg, tmp_buf, needed_space);
            _WRITE_STR(tmp_buf, needed_space - 1);
            free(tmp_buf);
            if (write_failed) return -1;
            chr++;
            continue;
        }
        
        int has_star_width = 0;
        int star_width = 0;

        int has_star_prec = 0;
        int star_prec;

        while (*chr == '-' || *chr == '+' || *chr == ' ' || *chr == '#' || *chr == '0') chr++;
        
        if (*chr == '*') {
            has_star_width = 1;
            star_width = va_arg(*args, int);
            chr++;
        } else {
            while (isdigit(*chr)) chr++;
        }

        if (*chr == '.') {
            chr++;
            if (*chr == '*') {
                has_star_prec = 1;
                star_prec = va_arg(*args, int);
                chr++;
            } else {
                while (isdigit(*chr)) chr++;
            }
        }
        
        enum {
            LEN_NONE, LEN_HH, LEN_H, LEN_L, LEN_LL,
            LEN_J, LEN_Z, LEN_T, LEN_CAP_L
        } len = LEN_NONE;

        if (chr[0] == 'h' && chr[1] == 'h')         { len = LEN_HH; chr += 2; }
        else if (chr[0] == 'h')                     { len = LEN_H; chr++; }
        else if (chr[0] == 'l' && chr[1] == 'l')    { len = LEN_LL; chr += 2; }
        else if (chr[0] == 'l')                     { len = LEN_L; chr++; }
        else if (chr[0] == 'j')                     { len = LEN_J; chr++; }
        else if (chr[0] == 'z')                     { len = LEN_Z; chr++; }
        else if (chr[0] == 't')                     { len = LEN_T; chr++; }
        else if (chr[0] == 'L')                     { len = LEN_CAP_L; chr++; }

        char spec = *chr++;

        size_t spec_len = (size_t)(chr - fmt_start);
        char *spec_fmt_str = malloc((spec_len + 1) * sizeof(char));
        memcpy(spec_fmt_str, fmt_start, (spec_len + 1) * sizeof(char));
        spec_fmt_str[spec_len] = '\0';

        switch (spec) {
            case 'd': case 'i':
            switch (len) {
                case LEN_HH: { _RENDER_FMT_SPEC(int); break; } // Actually a signed char
                case LEN_H:  { _RENDER_FMT_SPEC(int); break; } // Actually a short
                case LEN_NONE: { _RENDER_FMT_SPEC(int); break; }
                case LEN_L: { _RENDER_FMT_SPEC(long); break; }
                case LEN_LL: { _RENDER_FMT_SPEC(long long); break; }
                case LEN_J: { _RENDER_FMT_SPEC(intmax_t); break; }
                case LEN_Z: { _RENDER_FMT_SPEC(size_t); break; }
                case LEN_T: { _RENDER_FMT_SPEC(ptrdiff_t); break; }
                default: UNREACHABLE("Invalid length specifier");
            }
            break;

        case 'u': case 'o': case 'x': case 'X':
            switch (len) {
                case LEN_HH: { _RENDER_FMT_SPEC(unsigned int); break; } // Actually a unsigned char
                case LEN_H: { _RENDER_FMT_SPEC(unsigned int); break; }  // Actually a unsigned short
                case LEN_NONE: { _RENDER_FMT_SPEC(unsigned int); break; }
                case LEN_L: { _RENDER_FMT_SPEC(unsigned long); break; }
                case LEN_LL: { _RENDER_FMT_SPEC(unsigned long long); break; }
                case LEN_J: { _RENDER_FMT_SPEC(uintmax_t); break; }
                case LEN_Z: { _RENDER_FMT_SPEC(size_t); break; }
                case LEN_T: { _RENDER_FMT_SPEC(ptrdiff_t); break; }
                default: UNREACHABLE("Invalid length specifier");
            }
            break;

        /* ---------------- floats ---------------- */
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A':
            if (len == LEN_CAP_L) { _RENDER_FMT_SPEC(long double); } 
            else { _RENDER_FMT_SPEC(double); }
            break;

        /* ---------------- char ---------------- */
        case 'c':
            if (len == LEN_L) { _RENDER_FMT_SPEC(wint_t); } 
            else { _RENDER_FMT_SPEC(int); } // Actually a char
            break;

        /* ---------------- string ---------------- */
        case 's':
            if (len == LEN_L) { _RENDER_FMT_SPEC(wchar_t*); } 
            else { _RENDER_FMT_SPEC(char*); }
            break;

        /* ---------------- pointer ---------------- */
        case 'p':
        _RENDER_FMT_SPEC(void*);
            break;

        /* ---------------- n spec ---------------- */
        case 'n':
            switch (len) {
                // We dont actually have to care about the type here since all of them are pointers who all have the same size
                case LEN_HH: { _RENDER_FMT_SPEC(signed char*); break; } // signed char *
                case LEN_H: { _RENDER_FMT_SPEC(short*); break; }     // short *
                case LEN_NONE: { _RENDER_FMT_SPEC(int*); break; }     // int *
                case LEN_L: { _RENDER_FMT_SPEC(long*); break; }       // long *
                case LEN_LL: { _RENDER_FMT_SPEC(long long*); break; }  // long long *
                case LEN_J: { _RENDER_FMT_SPEC(intmax_t*); break; }   // intmax_t *
                case LEN_Z: { _RENDER_FMT_SPEC(size_t*); break; }     // size_t *
                case LEN_T: { _RENDER_FMT_SPEC(ptrdiff_t*); break; }  // prtdiff_t *
                default: UNREACHABLE("Invalid length specifier");

            }
            break;
        default:
            axp_throw(ctx, AXP_ERR_FORMAT, "Unknown specifier: %%%c.", spec);
            return -1;
        }
    }

    #undef _WRITE_STR
    #undef _WRITE_FMT_SPEC
    #undef _RENDER_FMT_SPEC

    axp_error_reset(ctx);
    
    return out_pos;
}

static int axp__write_to_fd(AXP_Ctx *ctx, void *file_desc, const char *txt, size_t txt_len, int *curr_pos) {
    FILE *fd = *(FILE **)file_desc;
    size_t written = fwrite(txt, 1, txt_len, fd);
    if (written < txt_len && ferror(fd)) {
        axp_throw(ctx, AXP_ERR_WRITE, "Could not write to file descriptor: %s", strerror(errno));
        return -1;
    }
    *curr_pos += written;
    return (int)written;
}

static int axp__write_to_buf(AXP_Ctx *ctx, void *buffer, const char *txt, size_t txt_len, int *curr_pos) {
    (void)ctx;
    struct {
        char *buf;
        size_t buf_sz;
    } *b = buffer;
    if ((size_t)*curr_pos < b->buf_sz) {
        size_t _actual_length = (txt_len < b->buf_sz - (size_t)*curr_pos) ? txt_len : (b->buf_sz - (size_t)*curr_pos);
        if (b->buf) memcpy(b->buf + *curr_pos, txt, _actual_length);
    }
    *curr_pos += (int)txt_len;
    return (int)txt_len;
}

int axp_vsnprintf(AXP_Ctx *ctx, char *buf, size_t buf_sz, const char *fmt, va_list *args) {
    axp__print_target target = { 0 };
    target.type = AXP__TARGET_BUF;
    target.as.buf = buf;
    target.as.buf_sz = buf_sz;
    target.write_fn = axp__write_to_buf;
    return axp__printf_core(ctx, &target, fmt, args);
}

int axp_vfprintf(AXP_Ctx *ctx, FILE *stream, const char *fmt, va_list *args) {
    axp__print_target target = { 0 };
    target.type = AXP__TARGET_FD;
    target.as.fd = stream;
    target.write_fn = axp__write_to_fd;
    return axp__printf_core(ctx, &target, fmt, args);
}

int axp_snprintf(AXP_Ctx *ctx, char *buf, size_t buf_sz, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    axp__print_target target = { 0 };
    target.type = AXP__TARGET_BUF;
    target.as.buf = buf;
    target.as.buf_sz = buf_sz;
    target.write_fn = axp__write_to_buf;
    int ret = axp__printf_core(ctx, &target, fmt, &args);
    va_end(args);
    return ret;
}

int axp_fprintf(AXP_Ctx *ctx, FILE *stream, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    axp__print_target target = { 0 };
    target.type = AXP__TARGET_FD;
    target.as.fd = stream;
    target.write_fn = axp__write_to_fd;
    int ret = axp__printf_core(ctx, &target, fmt, &args);
    va_end(args);
    return ret;
}

int axp_printf(AXP_Ctx *ctx, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    axp__print_target target = { 0 };
    target.type = AXP__TARGET_FD;
    target.as.fd = stdout;
    target.write_fn = axp__write_to_fd;
    int ret = axp__printf_core(ctx, &target, fmt, &args);
    va_end(args);
    return ret;
}
