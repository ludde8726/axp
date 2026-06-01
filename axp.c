#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>

#include "axp.h"


// 
// ------- Helper functions -------

#define AXP_TYPE_MIN_VALUE(T) \
    ( (T)(1ULL << (sizeof(T)*CHAR_BIT - 1)) )

#define AXP_TYPE_MAX_VALUE(T) \
    ( (T)~(T)(1ULL << (sizeof(T)*CHAR_BIT - 1)) )

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

bool axp_is_zeroi(AXP_Ctx *ctx, const AXP_Int *x, bool *res) {
    if (!x->digits) {
        axp_throw(ctx, AXP_ERR_UNINITIALIZED, "Cannot check value of uninitalized integer.");
        return false;
    }
    *res = (x->size == 0 || ((x->size == 1) && (x->digits[0] == 0)));
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
    // This does not support differently signed operands
    if (!axp_initf_ex(ctx, res, ctx->precision + 1)) return false;
    AXP_Float x_cpy = { 0 };
    AXP_Float y_cpy = { 0 };
    if (!axp_copyf_ex(ctx, &x_cpy, x, ctx->precision + 1)) goto cleanup_error;
    if (!axp_copyf_ex(ctx, &y_cpy, y, ctx->precision + 1)) goto cleanup_success;

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
        
        if (!axp_reallocf(ctx, res, ctx->precision)) goto cleanup_error; // This has to be a rounded resize which is not yet implemented
        goto cleanup_success;
    }

    axp_size_t res_sz = axp__add_digits(x_cpy.digits, x_cpy.size, y_cpy.digits, y_cpy.size, res->digits);
    res->size = res_sz;
    res->exponent = x_cpy.exponent;
    res->sign = x_cpy.sign;
    axp_normalizef(res);
    if (!axp_reallocf(ctx, res, ctx->precision)) goto cleanup_error; // This has to be a rounded resize which is not yet implemented
    
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
    while (res[res_sz-1] == 0) res_sz--;
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
    if (!axp_copyf_ex(ctx, &x_cpy, x, ctx->precision + 1)) goto cleanup_error;
    if (!axp_copyf_ex(ctx, &y_cpy, y, ctx->precision + 1)) goto cleanup_success;

    axp_align_float_digits(&x_cpy, &y_cpy);

    if (x_cpy.sign != y_cpy.sign) {
        axp_size_t res_sz = axp__add_digits(x_cpy.digits, x_cpy.size, y_cpy.digits, y_cpy.size, res->digits);
        res->exponent = x_cpy.exponent;
        res->size = res_sz;
        res->sign = x_cpy.sign;
        res->exponent = x_cpy.exponent;
        axp_normalizef(res);
        if (!axp_reallocf(ctx, res, ctx->precision)) goto cleanup_error; 
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
    
    if (!axp_reallocf(ctx, res, ctx->precision)) goto cleanup_error;

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
    if (!axp_copyi_ex(ctx, &y_copy, y, x->size)) goto cleanup_error;
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
        remainder->size = remainder_sz;
        remainder->sign = x->sign;
    }
    else axp_freei(&x_copy);
    axp_freei(&y_copy);
    return true;
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
    if (is_zero && ! (y == 0)) {
        if (!axp_copyi(ctx, res, x)) return false;
    } else if (y == 0) {
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
    [AXP_ERR_UNINITIALIZED] = "Uninitialized number error"
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
