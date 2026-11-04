#include <math.h>
#include <stdio.h>
#include "ladrc.h"

static int failures;

static void check(int condition, const char *name) {
    if (!condition) {
        printf("FAIL %s\n", name);
        ++failures;
    }
}

static void near_value(float actual, float expected, float tolerance,
                       const char *name) {
    if (!isfinite(actual) || fabsf(actual - expected) > tolerance) {
        printf("FAIL %s: expected %.8g, got %.8g\n", name, expected, actual);
        ++failures;
    }
}

static void check_first_safe(const first_order_ladrc_t *f, const char *name) {
    check(f->b == 0.0f && f->dt == 0.0f && f->max_output == 0.0f, name);
    check(f->x1 == 0.0f && f->x2 == 0.0f && f->pre_out == 0.0f &&
          f->out == 0.0f && !f->use_td, name);
    check(f->td.r == 0.0f && f->td.h == 0.0f && f->td.h0 == 0.0f &&
          f->td.x1 == 0.0f && f->td.x2 == 0.0f, name);
}

static void check_second_safe(const ladrc_t *l, const char *name) {
    check(l->b == 0.0f && l->dt == 0.0f && l->max_output == 0.0f, name);
    check(l->x1 == 0.0f && l->x2 == 0.0f && l->x3 == 0.0f &&
          l->pre_out == 0.0f && l->out == 0.0f && !l->use_td, name);
    check(l->td.r == 0.0f && l->td.h == 0.0f && l->td.h0 == 0.0f &&
          l->td.x1 == 0.0f && l->td.x2 == 0.0f, name);
}

static void test_valid_initialization(void) {
    first_order_ladrc_t f;
    ladrc_t l;

    first_order_ladrc_init(&f, 10.0f, 2.0f, 1.0f, 3.0f, 2.0f,
                           0.01f, 2.0f, 0.0f, 2.0f, 0.0f);
    check(!f.use_td && f.td.r == 0.0f, "first TD initially off");
    near_value(f.td.h, 0.01f, 1e-7f, "first off TD h initialized");
    near_value(f.td.h0, 0.02f, 1e-7f, "first off TD h0 initialized");
    check(f.x1 == 0.0f && f.x2 == 0.0f && f.out == 0.0f,
          "first valid initial states");
    near_value(first_order_ladrc_calc(&f, 1.0f, 0.0f), 1.5f, 1e-6f,
               "first valid calculation");

    ladrc_init(&l, 10.0f, 3.0f, 3.0f, 1.0f, 4.0f, 1.0f, 2.0f,
               0.01f, 2.0f, 0.0f, 3.0f, 0.0f);
    check(!l.use_td && l.td.r == 0.0f, "second TD initially off");
    near_value(l.td.h, 0.01f, 1e-7f, "second off TD h initialized");
    near_value(l.td.h0, 0.03f, 1e-7f, "second off TD h0 initialized");
    check(l.x1 == 0.0f && l.x2 == 0.0f && l.x3 == 0.0f && l.out == 0.0f,
          "second valid initial states");
    near_value(ladrc_calc(&l, 1.0f, 0.0f), 2.0f, 1e-6f,
               "second valid calculation");

    first_order_ladrc_init(&f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f,
                           0.01f, 0.0f, 0.0f, 1.0f, 0.0f);
    near_value(first_order_ladrc_calc(&f, 100.0f, 0.0f), 0.0f, 0.0f,
               "first zero output limit is valid");
    ladrc_init(&l, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
               0.01f, 0.0f, 0.0f, 1.0f, 0.0f);
    near_value(ladrc_calc(&l, 100.0f, 0.0f), 0.0f, 0.0f,
               "second zero output limit is valid");
}

static void test_invalid_initialization(void) {
    first_order_ladrc_t f;
    ladrc_t l;

    first_order_ladrc_init(&f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                           0.01f, 0.0f, 0.0f, 1.0f, 0.0f);
    check_first_safe(&f, "first reject zero b");
    near_value(first_order_ladrc_calc(&f, 1.0f, 0.0f), 0.0f, 0.0f,
               "first zero b safe output");
    first_order_ladrc_init(&f, 1.0f, 0.0f, 0.0f, 1.0f, INFINITY,
                           0.01f, 0.0f, 0.0f, 1.0f, 0.0f);
    check_first_safe(&f, "first reject infinite b");
    first_order_ladrc_init(&f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
                           0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    check_first_safe(&f, "first reject zero dt");
    first_order_ladrc_init(&f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
                           NAN, 0.0f, 0.0f, 1.0f, 0.0f);
    check_first_safe(&f, "first reject NaN dt");
    first_order_ladrc_init(&f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
                           0.01f, 0.0f, 0.0f, 1.0f, 0.0f);
    check_first_safe(&f, "first reject negative max output");
    first_order_ladrc_init(&f, INFINITY, 0.0f, 0.0f, 1.0f, 1.0f,
                           0.01f, 0.0f, 0.0f, 1.0f, 0.0f);
    check_first_safe(&f, "first reject infinite max output");
    first_order_ladrc_init(&f, 1.0f, NAN, 0.0f, 1.0f, 1.0f,
                           0.01f, 0.0f, 0.0f, 1.0f, 0.0f);
    check_first_safe(&f, "first reject nonfinite parameter");

    ladrc_init(&l, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
               0.01f, 0.0f, 0.0f, 1.0f, 0.0f);
    check_second_safe(&l, "second reject zero b");
    near_value(ladrc_calc(&l, 1.0f, 0.0f), 0.0f, 0.0f,
               "second zero b safe output");
    ladrc_init(&l, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, NAN,
               0.01f, 0.0f, 0.0f, 1.0f, 0.0f);
    check_second_safe(&l, "second reject NaN b");
    ladrc_init(&l, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
               -0.01f, 0.0f, 0.0f, 1.0f, 0.0f);
    check_second_safe(&l, "second reject negative dt");
    ladrc_init(&l, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
               INFINITY, 0.0f, 0.0f, 1.0f, 0.0f);
    check_second_safe(&l, "second reject infinite dt");
    ladrc_init(&l, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
               0.01f, 0.0f, 0.0f, 1.0f, 0.0f);
    check_second_safe(&l, "second reject negative max output");
    ladrc_init(&l, NAN, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
               0.01f, 0.0f, 0.0f, 1.0f, 0.0f);
    check_second_safe(&l, "second reject NaN max output");
    ladrc_init(&l, 1.0f, 0.0f, 0.0f, INFINITY, 1.0f, 0.0f, 1.0f,
               0.01f, 0.0f, 0.0f, 1.0f, 0.0f);
    check_second_safe(&l, "second reject nonfinite parameter");

    first_order_ladrc_init(&f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
                           0.01f, 0.0f, 0.0f, 1.0f, 0.0f);
    first_order_ladrc_reset(&f, 0.0f, 0.0f, 1.0f, 0.0f,
                            0.01f, 0.0f, 0.0f, 1.0f, 0.0f);
    check_first_safe(&f, "first reset rejects zero b");
    ladrc_init(&l, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
               0.01f, 0.0f, 0.0f, 1.0f, 0.0f);
    ladrc_reset(&l, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
                0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    check_second_safe(&l, "second reset rejects zero dt");
}

static void test_nonfinite_inputs_and_parameters(void) {
    first_order_ladrc_t f;
    ladrc_t l;

    first_order_ladrc_init(&f, 5.0f, 0.0f, 0.0f, 1.0f, 1.0f,
                           0.01f, 0.0f, 0.0f, 1.0f, 0.0f);
    near_value(first_order_ladrc_calc(&f, 1.0f, 0.0f), 1.0f, 1e-6f,
               "first finite baseline");
    near_value(first_order_ladrc_calc(&f, 1.0f, NAN), 0.0f, 0.0f,
               "first NaN measurement");
    check(isfinite(f.x1) && isfinite(f.x2) && f.pre_out == 0.0f,
          "first NaN measurement preserves finite state");
    near_value(first_order_ladrc_calc(&f, 1.0f, INFINITY), 0.0f, 0.0f,
               "first infinite measurement");
    near_value(first_order_ladrc_calc(&f, NAN, 0.0f), 0.0f, 0.0f,
               "first NaN target");
    near_value(first_order_ladrc_calc(&f, 1.0f, 0.0f), 1.0f, 1e-6f,
               "first recovery after nonfinite input");
    f.b = 0.0f;
    near_value(first_order_ladrc_calc(&f, 1.0f, 0.0f), 0.0f, 0.0f,
               "first runtime zero b");
    f.b = 1.0f;
    f.kp = NAN;
    near_value(first_order_ladrc_calc(&f, 1.0f, 0.0f), 0.0f, 0.0f,
               "first runtime NaN parameter");

    ladrc_init(&l, 5.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
               0.01f, 0.0f, 0.0f, 1.0f, 0.0f);
    near_value(ladrc_calc(&l, 1.0f, 0.0f), 1.0f, 1e-6f,
               "second finite baseline");
    near_value(ladrc_calc(&l, 1.0f, NAN), 0.0f, 0.0f,
               "second NaN measurement");
    check(isfinite(l.x1) && isfinite(l.x2) && isfinite(l.x3) && l.pre_out == 0.0f,
          "second NaN measurement preserves finite state");
    near_value(ladrc_calc(&l, 1.0f, -INFINITY), 0.0f, 0.0f,
               "second infinite measurement");
    near_value(ladrc_calc(&l, INFINITY, 0.0f), 0.0f, 0.0f,
               "second infinite target");
    near_value(ladrc_calc(&l, 1.0f, 0.0f), 1.0f, 1e-6f,
               "second recovery after nonfinite input");
    l.dt = NAN;
    near_value(ladrc_calc(&l, 1.0f, 0.0f), 0.0f, 0.0f,
               "second runtime NaN dt");
}

static void test_td_switching_and_defense(void) {
    td_t td;
    first_order_ladrc_t f;
    ladrc_t l;

    td_init(&td, 0.0f, 0.01f, 2.0f, 0.0f);
    near_value(td_update(&td, 2.0f), 2.0f, 0.0f, "TD off passthrough");
    check(td.x1 == 2.0f && td.x2 == 0.0f, "TD off synchronizes state");
    td_init(&td, 5.0f, 0.01f, 2.0f, 0.0f);
    check(td_update(&td, 1.0f) != 1.0f, "TD on tracks instead of passthrough");
    td.r = 0.0f;
    near_value(td_update(&td, 0.25f), 0.25f, 0.0f, "TD online r zero disables");
    td.r = -1.0f;
    near_value(td_update(&td, -0.5f), -0.5f, 0.0f,
               "TD online negative r disables");
    near_value(td_update(&td, NAN), 0.0f, 0.0f, "TD rejects NaN target");
    td_init(&td, 5.0f, 0.01f, 2.0f, 0.0f);
    td.h = 0.0f;
    near_value(td_update(&td, 1.0f), 0.0f, 0.0f, "TD rejects zero h");
    check(td.h == 0.0f && td.x1 == 0.0f && td.x2 == 0.0f,
          "TD invalid h safe state");
    td_init(&td, 5.0f, 0.0f, 2.0f, 0.0f);
    near_value(td_update(&td, 1.0f), 0.0f, 0.0f, "TD invalid init safe output");
    near_value(td_fhan(1.0f, 0.0f, 0.0f, 0.01f, 0.02f), 0.0f, 0.0f,
               "fhan rejects zero r");
    near_value(td_fhan(1.0f, 0.0f, 1.0f, NAN, 0.02f), 0.0f, 0.0f,
               "fhan rejects NaN h");

    first_order_ladrc_init(&f, 10.0f, 0.0f, 0.0f, 2.0f, 1.0f,
                           0.01f, 0.0f, 0.0f, 2.0f, 0.0f);
    first_order_ladrc_reset(&f, 0.0f, 0.0f, 2.0f, 1.0f,
                            0.01f, 0.0f, 5.0f, 3.0f, 0.0f);
    check(f.use_td && f.td.r == 5.0f && f.td.x1 == 0.0f && f.td.x2 == 0.0f,
          "first TD off to on fully initializes");
    check(isfinite(first_order_ladrc_calc(&f, 1.0f, 0.0f)),
          "first TD on output finite");
    first_order_ladrc_reset(&f, 0.0f, 0.0f, 2.0f, 1.0f,
                            0.02f, 0.0f, 0.0f, 4.0f, 0.0f);
    check(!f.use_td && f.td.r == 0.0f && f.td.x1 == 0.0f && f.td.x2 == 0.0f,
          "first TD on to off resets state");
    near_value(f.td.h0, 0.08f, 1e-7f, "first off TD parameters complete");
    near_value(first_order_ladrc_calc(&f, 1.0f, 0.0f), 2.0f, 1e-6f,
               "first TD off target passthrough");

    ladrc_init(&l, 10.0f, 0.0f, 0.0f, 0.0f, 2.0f, 0.0f, 1.0f,
               0.01f, 0.0f, 0.0f, 2.0f, 0.0f);
    ladrc_reset(&l, 0.0f, 0.0f, 0.0f, 2.0f, 0.0f, 1.0f,
                0.01f, 0.0f, 5.0f, 3.0f, 0.0f);
    check(l.use_td && l.td.r == 5.0f && l.td.x1 == 0.0f && l.td.x2 == 0.0f,
          "second TD off to on fully initializes");
    check(isfinite(ladrc_calc(&l, 1.0f, 0.0f)), "second TD on output finite");
    ladrc_reset(&l, 0.0f, 0.0f, 0.0f, 2.0f, 0.0f, 1.0f,
                0.02f, 0.0f, 0.0f, 4.0f, 0.0f);
    check(!l.use_td && l.td.r == 0.0f && l.td.x1 == 0.0f && l.td.x2 == 0.0f,
          "second TD on to off resets state");
    near_value(l.td.h0, 0.08f, 1e-7f, "second off TD parameters complete");
    near_value(ladrc_calc(&l, 1.0f, 0.0f), 2.0f, 1e-6f,
               "second TD off target passthrough");
}

static void test_repeated_reset(void) {
    first_order_ladrc_t f;
    ladrc_t l;

    first_order_ladrc_init(&f, 10.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                           0.01f, 0.0f, 5.0f, 2.0f, 0.0f);
    (void)first_order_ladrc_calc(&f, 2.0f, 1.0f);
    for (int i = 0; i < 3; ++i) {
        first_order_ladrc_reset(&f, 1.0f, 1.0f, 1.0f, 1.0f,
                                0.01f, 0.0f, 5.0f, 2.0f, 0.0f);
        check(f.x1 == 0.0f && f.x2 == 0.0f && f.pre_out == 0.0f &&
              f.out == 0.0f && f.td.x1 == 0.0f && f.td.x2 == 0.0f,
              "first repeated reset clears all state");
    }

    ladrc_init(&l, 10.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
               0.01f, 0.0f, 5.0f, 2.0f, 0.0f);
    (void)ladrc_calc(&l, 2.0f, 1.0f);
    for (int i = 0; i < 3; ++i) {
        ladrc_reset(&l, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                    0.01f, 0.0f, 5.0f, 2.0f, 0.0f);
        check(l.x1 == 0.0f && l.x2 == 0.0f && l.x3 == 0.0f &&
              l.pre_out == 0.0f && l.out == 0.0f &&
              l.td.x1 == 0.0f && l.td.x2 == 0.0f,
              "second repeated reset clears all state");
    }
}

static void test_saturation_output_and_previous_input(void) {
    first_order_ladrc_t f;
    ladrc_t l;

    first_order_ladrc_init(&f, 2.0f, 0.0f, 0.0f, 10.0f, 1.0f,
                           0.01f, 0.0f, 0.0f, 1.0f, 0.0f);
    near_value(first_order_ladrc_calc(&f, 100.0f, 0.0f), 2.0f, 0.0f,
               "first positive saturation");
    near_value(first_order_ladrc_calc(&f, -100.0f, 0.0f), -2.0f, 0.0f,
               "first negative saturation");
    for (int i = 0; i < 100; ++i) {
        float target = (i & 1) ? -1e20f : 1e20f;
        float output = first_order_ladrc_calc(&f, target, 0.0f);
        check(isfinite(output) && fabsf(output) <= 2.0f,
              "first large command output finite and bounded");
    }

    ladrc_init(&l, 2.0f, 0.0f, 0.0f, 0.0f, 10.0f, 0.0f, 1.0f,
               0.01f, 0.0f, 0.0f, 1.0f, 0.0f);
    near_value(ladrc_calc(&l, 100.0f, 0.0f), 2.0f, 0.0f,
               "second positive saturation");
    near_value(ladrc_calc(&l, -100.0f, 0.0f), -2.0f, 0.0f,
               "second negative saturation");
    for (int i = 0; i < 100; ++i) {
        float target = (i & 1) ? -1e20f : 1e20f;
        float output = ladrc_calc(&l, target, 0.0f);
        check(isfinite(output) && fabsf(output) <= 2.0f,
              "second large command output finite and bounded");
    }

    first_order_ladrc_init(&f, 1.0f, 0.0f, 0.0f, 1.0f, 2.0f,
                           0.1f, 0.0f, 0.0f, 1.0f, 0.0f);
    near_value(first_order_ladrc_calc(&f, 10.0f, 0.0f), 1.0f, 0.0f,
               "first previous output setup");
    (void)first_order_ladrc_calc(&f, 10.0f, 0.0f);
    near_value(f.x1, 0.2f, 1e-6f, "first LESO uses previous limited u");

    ladrc_init(&l, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 2.0f,
               0.1f, 0.0f, 0.0f, 1.0f, 0.0f);
    near_value(ladrc_calc(&l, 10.0f, 0.0f), 1.0f, 0.0f,
               "second previous output setup");
    (void)ladrc_calc(&l, 0.0f, 0.0f);
    near_value(l.x2, 0.2f, 1e-6f, "second LESO uses previous limited u");
}

static void test_disturbance_and_tuning(void) {
    first_order_ladrc_t f;
    ladrc_t l;

    first_order_ladrc_init(&f, 100.0f, 0.0f, 5.0f, 0.0f, 1.0f,
                           0.1f, 0.0f, 0.0f, 1.0f, 0.0f);
    near_value(first_order_ladrc_calc(&f, 0.0f, 1.0f), -0.5f, 1e-6f,
               "first nonzero disturbance compensation");
    near_value(first_order_ladrc_get_disturbance(&f), 0.5f, 1e-6f,
               "first nonzero disturbance estimate");

    ladrc_init(&l, 100.0f, 0.0f, 0.0f, 5.0f, 0.0f, 0.0f, 1.0f,
               0.1f, 0.0f, 0.0f, 1.0f, 0.0f);
    near_value(ladrc_calc(&l, 0.0f, 1.0f), -0.5f, 1e-6f,
               "second nonzero disturbance compensation");
    near_value(ladrc_get_disturbance(&l), 0.5f, 1e-6f,
               "second nonzero disturbance estimate");

    first_order_ladrc_tune(&f, 3.0f, 4.0f, 2.0f);
    near_value(f.beta1, 6.0f, 0.0f, "first tune beta1");
    near_value(f.beta2, 9.0f, 0.0f, "first tune beta2");
    near_value(f.kp, 4.0f, 0.0f, "first tune kp");
    near_value(f.b, 2.0f, 0.0f, "first tune b");

    ladrc_tune(&l, 3.0f, 4.0f, 2.0f);
    near_value(l.beta1, 9.0f, 0.0f, "second tune beta1");
    near_value(l.beta2, 27.0f, 0.0f, "second tune beta2");
    near_value(l.beta3, 27.0f, 0.0f, "second tune beta3");
    near_value(l.kp, 16.0f, 0.0f, "second tune kp");
    near_value(l.kd, 8.0f, 0.0f, "second tune kd");
    near_value(l.b, 2.0f, 0.0f, "second tune b");
}

static void test_k_aw_compatibility_only(void) {
    first_order_ladrc_t f0;
    first_order_ladrc_t f1;
    ladrc_t l0;
    ladrc_t l1;

    first_order_ladrc_init(&f0, 1.0f, 1.0f, 1.0f, 5.0f, 1.0f,
                           0.01f, 0.0f, 0.0f, 1.0f, 0.0f);
    first_order_ladrc_init(&f1, 1.0f, 1.0f, 1.0f, 5.0f, 1.0f,
                           0.01f, 10.0f, 0.0f, 1.0f, 0.0f);
    ladrc_init(&l0, 1.0f, 1.0f, 1.0f, 1.0f, 5.0f, 1.0f, 1.0f,
               0.01f, 0.0f, 0.0f, 1.0f, 0.0f);
    ladrc_init(&l1, 1.0f, 1.0f, 1.0f, 1.0f, 5.0f, 1.0f, 1.0f,
               0.01f, 10.0f, 0.0f, 1.0f, 0.0f);
    for (int i = 0; i < 10; ++i) {
        float target = (i & 1) ? -10.0f : 10.0f;
        near_value(first_order_ladrc_calc(&f1, target, 0.0f),
                   first_order_ladrc_calc(&f0, target, 0.0f), 0.0f,
                   "first k_aw currently has no effect");
        near_value(ladrc_calc(&l1, target, 0.0f),
                   ladrc_calc(&l0, target, 0.0f), 0.0f,
                   "second k_aw currently has no effect");
    }
}

static void test_reference_velocity_feedforward(void) {
    ladrc_t without_kd;
    ladrc_t with_kd;
    ladrc_init(&without_kd, 100.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
               0.01f, 0.0f, 5.0f, 2.0f, 0.0f);
    ladrc_init(&with_kd, 100.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f,
               0.01f, 0.0f, 5.0f, 2.0f, 0.0f);
    float baseline = ladrc_calc(&without_kd, 1.0f, 0.0f);
    float output = ladrc_calc(&with_kd, 1.0f, 0.0f);
    check(with_kd.td.x2 != 0.0f, "TD velocity test is active");
    near_value(output, baseline + with_kd.td.x2 - with_kd.x2, 1e-6f,
               "second controller uses TD reference velocity");
}

int main(void) {
    test_valid_initialization();
    test_invalid_initialization();
    test_nonfinite_inputs_and_parameters();
    test_td_switching_and_defense();
    test_repeated_reset();
    test_saturation_output_and_previous_input();
    test_disturbance_and_tuning();
    test_k_aw_compatibility_only();
    test_reference_velocity_feedforward();

    printf("LADRC host checks: %d failures\n", failures);
    return failures ? 1 : 0;
}
