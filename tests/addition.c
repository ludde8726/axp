#include <stdio.h>
#include <stdlib.h>

#include "axp.h"

void axp_printi(AXP_Int *x) {
    if (x->sign) printf("-");
    for (axp_size_t i = x->size; i > 0; i--) printf("%u", x->digits[i - 1]);
}

void add_from_str(AXP_Ctx *ctx, const char *x_str, const char *y_str) {
    AXP_Int x;
    if (!axp_initi_from_str(ctx, x_str, &x)) {
        fprintf(stderr, "ERROR: %s\n", axp_strerror(ctx));
        exit(1);
    }
    AXP_Int y;
    if (!axp_initi_from_str(ctx, y_str, &y)) {
        fprintf(stderr, "ERROR: %s\n", axp_strerror(ctx));
        exit(1);
    }
    AXP_Int res;
    if (!axp_addi(ctx, &x, &y, &res)) {
        fprintf(stderr, "ERROR: %s\n", axp_strerror(ctx));
        exit(1);
    }
    axp_printi(&res);
    printf("\n");
    axp_freei(&x);
    axp_freei(&y);
    axp_freei(&res);
}

int main() {
    AXP_Ctx ctx = {
        .precision = 16
    };
    add_from_str(&ctx, "1", "2");      // 3
    add_from_str(&ctx, "2", "5");      // 7
    add_from_str(&ctx, "7", "3");      // 10
    add_from_str(&ctx, "10", "33");    // 43
    add_from_str(&ctx, "0", "33");     // 33
    add_from_str(&ctx, "5", "-1");     // 4
    add_from_str(&ctx, "-1", "5");     // 4
    add_from_str(&ctx, "-1", "-5");    // -6
    add_from_str(&ctx, "-10", "2");    // -8
    add_from_str(&ctx, "2", "-10");    // -8

    return 0;
}