#include <stdio.h>
#include <stdlib.h>

#include "cxp.h"

void cxp_printi(CXP_Int *x) {
    if (x->sign) printf("-");
    for (cxp_size_t i = x->size; i > 0; i--) printf("%u", x->digits[i - 1]);
}

void add_from_str(CXP_Ctx *ctx, const char *x_str, const char *y_str) {
    CXP_Int x;
    if (!cxp_initi_from_str(ctx, x_str, &x)) {
        fprintf(stderr, "ERROR: %s\n", cxp_strerror(&ctx));
        exit(1);
    }
    CXP_Int y;
    if (!cxp_initi_from_str(ctx, y_str, &y)) {
        fprintf(stderr, "ERROR: %s\n", cxp_strerror(&ctx));
        exit(1);
    }
    CXP_Int res;
    if (!cxp_addi(ctx, &x, &y, &res)) {
        fprintf(stderr, "ERROR: %s\n", cxp_strerror(&ctx));
        exit(1);
    }
    cxp_printi(&res);
    printf("\n");
    cxp_freei(&x);
    cxp_freei(&y);
    cxp_freei(&res);
}

int main() {
    CXP_Ctx ctx = {
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