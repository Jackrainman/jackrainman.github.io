/**
 * @file    ladrc.c
 * @author  Jackrainman
 * @brief   LADRC算法库实现（包含一阶和二阶LADRC）
 * @version 3.2
 * @date    2026-11-04
 */

#include "ladrc.h"
#include <math.h>
#include <stddef.h>

/* ============================================================================
 *                          通用工具函数
 * ============================================================================ */

/**
 * @brief 输出限幅；非法数值一律转换为零，避免NaN穿过后续限幅
 */
static inline void abs_limit(float *a, float abs_max) {
    if (!isfinite(*a) || !isfinite(abs_max) || abs_max < 0.0f) {
        *a = 0.0f;
    } else if (*a > abs_max) {
        *a = abs_max;
    } else if (*a < -abs_max) {
        *a = -abs_max;
    }
}

/* ============================================================================
 *                          TD（跟踪微分器）实现
 * ============================================================================ */

/** TD非法初始化时进入确定的安全状态，td_update()将输出零。 */
static void td_safe_state(td_t *td) {
    td->r = 0.0f;
    td->h = 0.0f;
    td->h0 = 0.0f;
    td->max_x2 = 0.0f;
    td->x1 = 0.0f;
    td->x2 = 0.0f;
}

/** 检查完整TD配置；r<=0是合法的透传模式，但采样参数仍必须有效。 */
static int td_configuration_valid(const td_t *td) {
    return isfinite(td->r) &&
           isfinite(td->h) && td->h > 0.0f &&
           isfinite(td->h0) && td->h0 > 0.0f &&
           isfinite(td->max_x2) && td->max_x2 >= 0.0f;
}

/**
 * @brief TD 初始化 - 参数固化模式
 *
 * @param td      TD结构体指针
 * @param r       快速跟踪因子，决定跟踪速度；小于等于0表示禁用并透传
 * @param dt      固定采样周期(秒) - RTOS任务周期，将固化到结构体中
 * @param n       滤波因子(无量纲) - 建议值1~5，值越大滤波越强但延迟越大
 * @param max_x2  最大速度限制（0表示不限制）
 *
 * @note 无论是否启用TD，参数和状态都会完整初始化。n小于1时按1处理；
 *       非有限参数、dt<=0或max_x2<0会进入全零安全状态。
 */
void td_init(td_t *td, float r, float dt, float n, float max_x2) {
    if (td == NULL) {
        return;
    }
    if (!isfinite(r) || !isfinite(dt) || !isfinite(n) || !isfinite(max_x2) ||
        dt <= 0.0f || max_x2 < 0.0f) {
        td_safe_state(td);
        return;
    }

    if (r < 0.0f) {
        r = 0.0f;
    }
    if (n < 1.0f) {
        n = 1.0f;
    }

    td->r = r;
    td->h = dt;
    td->h0 = n * dt;
    td->max_x2 = max_x2;
    td->x1 = 0.0f;
    td->x2 = 0.0f;

    if (!td_configuration_valid(td)) {
        td_safe_state(td);
    }
}

/**
 * @brief fhan 最速控制综合函数（梯形加速度曲线）
 *
 * @param x1 位置误差
 * @param x2 速度
 * @param r 快速跟踪因子（相当于最大加速度）
 * @param h 积分步长（采样周期）
 * @param h0 滤波因子
 * @return 加速度输出；非法参数或数值溢出时返回零
 */
float td_fhan(float x1, float x2, float r, float h, float h0) {
    if (!isfinite(x1) || !isfinite(x2) || !isfinite(r) ||
        !isfinite(h) || !isfinite(h0) || r <= 0.0f || h <= 0.0f || h0 < 0.0f) {
        return 0.0f;
    }

    float d = r * h;
    float d0 = h0 * d;
    if (!isfinite(d) || !isfinite(d0) || d <= 0.0f) {
        return 0.0f;
    }

    float y = x1 + h * x2;
    const float MAX_Y = 1e15f;
    if (isnan(y)) {
        return 0.0f;
    }
    if (fabsf(y) > MAX_Y) {
        y = (y > 0.0f) ? MAX_Y : -MAX_Y;
    }

    float sqrt_arg = d * d + 8.0f * r * fabsf(y);
    if (!isfinite(sqrt_arg) || sqrt_arg < 0.0f) {
        return 0.0f;
    }
    float a0 = sqrtf(sqrt_arg);

    float a;
    if (fabsf(y) <= d0) {
        a = x2 + y / h;
    } else {
        a = x2 + 0.5f * (a0 - d) * ((y > 0.0f) ? 1.0f : -1.0f);
    }
    if (!isfinite(a)) {
        return 0.0f;
    }

    float fhan_out;
    if (fabsf(a) <= d) {
        fhan_out = -r * a / d;
    } else {
        fhan_out = -r * ((a > 0.0f) ? 1.0f : -1.0f);
    }
    return isfinite(fhan_out) ? fhan_out : 0.0f;
}

/**
 * @brief TD 更新计算 - 参数固化模式
 *
 * @note r<=0时明确禁用TD并透传target；非有限输入或非法配置输出零。
 *       速度采用前向欧拉更新，位置沿用本拍更新后的速度。
 *       固定dt只固定名义积分步长，不能消除实际任务调度抖动。
 */
float td_update(td_t *td, float target) {
    if (td == NULL) {
        return 0.0f;
    }
    if (!isfinite(target)) {
        td_reset(td, 0.0f);
        return 0.0f;
    }
    if (!td_configuration_valid(td)) {
        td_safe_state(td);
        return 0.0f;
    }
    if (td->r <= 0.0f) {
        td->x1 = target;
        td->x2 = 0.0f;
        return target;
    }
    if (!isfinite(td->x1) || !isfinite(td->x2)) {
        td_reset(td, 0.0f);
    }

    float x1_error = td->x1 - target;
    float fh = td_fhan(x1_error, td->x2, td->r, td->h, td->h0);
    float new_x2 = td->x2 + td->h * fh;

    if (td->max_x2 > 0.0f) {
        if (new_x2 > td->max_x2) {
            new_x2 = td->max_x2;
        } else if (new_x2 < -td->max_x2) {
            new_x2 = -td->max_x2;
        }
    }

    float new_x1 = td->x1 + td->h * new_x2;
    if (!isfinite(fh) || !isfinite(new_x2) || !isfinite(new_x1)) {
        td_reset(td, 0.0f);
        return 0.0f;
    }

    td->x2 = new_x2;
    td->x1 = new_x1;
    return td->x1;
}

/** TD重置只清状态，保留完整参数；非有限初始值按零处理。 */
void td_reset(td_t *td, float init_value) {
    if (td == NULL) {
        return;
    }
    td->x1 = isfinite(init_value) ? init_value : 0.0f;
    td->x2 = 0.0f;
}

/* ============================================================================
 *                          一阶 LADRC 实现
 * ============================================================================ */

/** 非法初始化/重置使用确定的安全状态：禁止TD、清零输出并令配置无效。 */
static void first_order_ladrc_safe_state(first_order_ladrc_t *fladrc) {
    fladrc->beta1 = 0.0f;
    fladrc->beta2 = 0.0f;
    fladrc->kp = 0.0f;
    fladrc->b = 0.0f;
    fladrc->x1 = 0.0f;
    fladrc->x2 = 0.0f;
    fladrc->max_output = 0.0f;
    fladrc->k_aw = 0.0f;
    fladrc->pre_out = 0.0f;
    fladrc->dt = 0.0f;
    fladrc->out = 0.0f;
    fladrc->use_td = false;
    td_safe_state(&fladrc->td);
}

static int first_order_ladrc_configuration_valid(const first_order_ladrc_t *fladrc) {
    return isfinite(fladrc->max_output) && fladrc->max_output >= 0.0f &&
           isfinite(fladrc->beta1) && isfinite(fladrc->beta2) &&
           isfinite(fladrc->kp) && isfinite(fladrc->b) && fladrc->b != 0.0f &&
           isfinite(fladrc->dt) && fladrc->dt > 0.0f &&
           isfinite(fladrc->k_aw) && td_configuration_valid(&fladrc->td) &&
           (!fladrc->use_td || fladrc->td.r > 0.0f);
}

static float first_order_ladrc_safe_output(first_order_ladrc_t *fladrc) {
    if (!isfinite(fladrc->x1) || !isfinite(fladrc->x2)) {
        fladrc->x1 = 0.0f;
        fladrc->x2 = 0.0f;
    }
    fladrc->out = 0.0f;
    fladrc->pre_out = 0.0f;
    return 0.0f;
}

static int first_order_ladrc_arguments_valid(float max_output,
                                             float beta1, float beta2,
                                             float kp, float b, float dt,
                                             float k_aw, float td_r,
                                             float td_n, float td_max_x2) {
    return isfinite(max_output) && max_output >= 0.0f &&
           isfinite(beta1) && isfinite(beta2) && isfinite(kp) &&
           isfinite(b) && b != 0.0f && isfinite(dt) && dt > 0.0f &&
           isfinite(k_aw) && isfinite(td_r) && isfinite(td_n) &&
           isfinite(td_max_x2) && td_max_x2 >= 0.0f;
}

/** 一阶LADRC初始化；k_aw仅为兼容保留，当前不执行抗饱和回算。 */
void first_order_ladrc_init(first_order_ladrc_t *fladrc, float max_output,
                           float beta1, float beta2, float kp, float b,
                           float dt, float k_aw,
                           float td_r, float td_n, float td_max_x2) {
    if (fladrc == NULL) {
        return;
    }
    if (!first_order_ladrc_arguments_valid(max_output, beta1, beta2, kp, b,
                                           dt, k_aw, td_r, td_n, td_max_x2)) {
        first_order_ladrc_safe_state(fladrc);
        return;
    }

    fladrc->beta1 = beta1;
    fladrc->beta2 = beta2;
    fladrc->kp = kp;
    fladrc->b = b;
    fladrc->x1 = 0.0f;
    fladrc->x2 = 0.0f;
    fladrc->max_output = max_output;
    fladrc->k_aw = k_aw; /* 兼容字段，当前未启用 */
    fladrc->pre_out = 0.0f;
    fladrc->dt = dt;
    fladrc->out = 0.0f;

    /* 关闭TD时也完整初始化参数和状态，保证后续off->on重置安全。 */
    td_init(&fladrc->td, td_r > 0.0f ? td_r : 0.0f, dt, td_n, td_max_x2);
    fladrc->use_td = td_r > 0.0f;
    if (!first_order_ladrc_configuration_valid(fladrc)) {
        first_order_ladrc_safe_state(fladrc);
    }
}

/** 一阶LADRC参数重置；TD每次完整初始化并重置状态。 */
void first_order_ladrc_reset(first_order_ladrc_t *fladrc,
                             float beta1, float beta2, float kp, float b,
                             float dt, float k_aw,
                             float td_r, float td_n, float td_max_x2) {
    if (fladrc == NULL) {
        return;
    }
    if (!first_order_ladrc_arguments_valid(fladrc->max_output, beta1, beta2,
                                           kp, b, dt, k_aw, td_r, td_n,
                                           td_max_x2)) {
        first_order_ladrc_safe_state(fladrc);
        return;
    }

    fladrc->beta1 = beta1;
    fladrc->beta2 = beta2;
    fladrc->kp = kp;
    fladrc->b = b;
    fladrc->dt = dt;
    fladrc->k_aw = k_aw; /* 兼容字段，当前未启用 */
    fladrc->x1 = 0.0f;
    fladrc->x2 = 0.0f;
    fladrc->pre_out = 0.0f;
    fladrc->out = 0.0f;
    td_init(&fladrc->td, td_r > 0.0f ? td_r : 0.0f, dt, td_n, td_max_x2);
    fladrc->use_td = td_r > 0.0f;
    if (!first_order_ladrc_configuration_valid(fladrc)) {
        first_order_ladrc_safe_state(fladrc);
    }
}

/**
 * @brief 一阶LADRC计算函数
 *
 * @note ESO采用前向欧拉积分，使用上一拍实际限幅输出pre_out计算本拍状态；
 *       本拍输出限幅后才更新pre_out。固定dt不能消除实际调度抖动。
 */
float first_order_ladrc_calc(first_order_ladrc_t *fladrc,
                             float target, float measure) {
    if (fladrc == NULL) {
        return 0.0f;
    }
    if (!first_order_ladrc_configuration_valid(fladrc) ||
        !isfinite(target) || !isfinite(measure) ||
        !isfinite(fladrc->pre_out) || !isfinite(fladrc->x1) ||
        !isfinite(fladrc->x2)) {
        return first_order_ladrc_safe_output(fladrc);
    }

    float td_target = fladrc->use_td ? td_update(&fladrc->td, target) : target;
    if (!isfinite(td_target)) {
        return first_order_ladrc_safe_output(fladrc);
    }

    /* 二阶LESO：b*pre_out使用上一拍已施加的实际限幅输出。 */
    float error = measure - fladrc->x1;
    float dx1 = fladrc->x2 + fladrc->b * fladrc->pre_out + fladrc->beta1 * error;
    float dx2 = fladrc->beta2 * error;
    float new_x1 = fladrc->x1 + dx1 * fladrc->dt; /* 前向欧拉 */
    float new_x2 = fladrc->x2 + dx2 * fladrc->dt; /* 前向欧拉 */
    if (!isfinite(new_x1) || !isfinite(new_x2)) {
        return first_order_ladrc_safe_output(fladrc);
    }
    fladrc->x1 = new_x1;
    fladrc->x2 = new_x2;

    float u0 = fladrc->kp * (td_target - fladrc->x1);
    /* k_aw回算暂时禁用：保留字段/参数仅为源码兼容，不参与控制律。 */
    float out_temp = 0.0f;
    if (isfinite(fladrc->b) && fladrc->b != 0.0f) {
        out_temp = (u0 - fladrc->x2) / fladrc->b;
    }
    abs_limit(&out_temp, fladrc->max_output);

    fladrc->out = out_temp;
    fladrc->pre_out = out_temp;
    return fladrc->out;
}

/* ============================================================================
 *                          二阶 LADRC 实现
 * ============================================================================ */

/** 非法初始化/重置使用确定的安全状态：禁止TD、清零输出并令配置无效。 */
static void ladrc_safe_state(ladrc_t *ladrc) {
    ladrc->beta1 = 0.0f;
    ladrc->beta2 = 0.0f;
    ladrc->beta3 = 0.0f;
    ladrc->kp = 0.0f;
    ladrc->kd = 0.0f;
    ladrc->b = 0.0f;
    ladrc->x1 = 0.0f;
    ladrc->x2 = 0.0f;
    ladrc->x3 = 0.0f;
    ladrc->max_output = 0.0f;
    ladrc->k_aw = 0.0f;
    ladrc->pre_out = 0.0f;
    ladrc->dt = 0.0f;
    ladrc->out = 0.0f;
    ladrc->use_td = false;
    td_safe_state(&ladrc->td);
}

static int ladrc_configuration_valid(const ladrc_t *ladrc) {
    return isfinite(ladrc->max_output) && ladrc->max_output >= 0.0f &&
           isfinite(ladrc->beta1) && isfinite(ladrc->beta2) &&
           isfinite(ladrc->beta3) && isfinite(ladrc->kp) &&
           isfinite(ladrc->kd) && isfinite(ladrc->b) && ladrc->b != 0.0f &&
           isfinite(ladrc->dt) && ladrc->dt > 0.0f &&
           isfinite(ladrc->k_aw) && td_configuration_valid(&ladrc->td) &&
           (!ladrc->use_td || ladrc->td.r > 0.0f);
}

static float ladrc_safe_output(ladrc_t *ladrc) {
    if (!isfinite(ladrc->x1) || !isfinite(ladrc->x2) || !isfinite(ladrc->x3)) {
        ladrc->x1 = 0.0f;
        ladrc->x2 = 0.0f;
        ladrc->x3 = 0.0f;
    }
    ladrc->out = 0.0f;
    ladrc->pre_out = 0.0f;
    return 0.0f;
}

static int ladrc_arguments_valid(float max_output, float beta1, float beta2,
                                 float beta3, float kp, float kd, float b,
                                 float dt, float k_aw, float td_r, float td_n,
                                 float td_max_x2) {
    return isfinite(max_output) && max_output >= 0.0f &&
           isfinite(beta1) && isfinite(beta2) && isfinite(beta3) &&
           isfinite(kp) && isfinite(kd) && isfinite(b) && b != 0.0f &&
           isfinite(dt) && dt > 0.0f && isfinite(k_aw) && isfinite(td_r) &&
           isfinite(td_n) && isfinite(td_max_x2) && td_max_x2 >= 0.0f;
}

/**
 * @brief LADRC 初始化 (参数固化模式)
 *
 * @param max_output 控制量输出限幅，必须有限且大于等于0
 * @param kp         控制器比例增益 (刚度)
 * @param kd         控制器微分增益 (阻尼)
 * @param b          系统增益估计值，必须有限且非零
 * @param dt         名义固定采样周期(秒)，必须与实际任务周期匹配
 * @param k_aw       兼容保留参数，当前不启用抗饱和回算
 * @param td_r       TD快速跟踪因子；小于等于0表示禁用TD并直接透传
 */
void ladrc_init(ladrc_t *ladrc, float max_output,
                float beta1, float beta2, float beta3,
                float kp, float kd, float b,
                float dt, float k_aw,
                float td_r, float td_n, float td_max_x2) {
    if (ladrc == NULL) {
        return;
    }
    if (!ladrc_arguments_valid(max_output, beta1, beta2, beta3, kp, kd, b,
                               dt, k_aw, td_r, td_n, td_max_x2)) {
        ladrc_safe_state(ladrc);
        return;
    }

    ladrc->beta1 = beta1;
    ladrc->beta2 = beta2;
    ladrc->beta3 = beta3;
    ladrc->kp = kp;
    ladrc->kd = kd;
    ladrc->b = b;
    ladrc->x1 = 0.0f;
    ladrc->x2 = 0.0f;
    ladrc->x3 = 0.0f;
    ladrc->max_output = max_output;
    ladrc->k_aw = k_aw; /* 兼容字段，当前未启用 */
    ladrc->pre_out = 0.0f;
    ladrc->out = 0.0f;
    ladrc->dt = dt;

    /* 关闭TD时也完整初始化参数和状态，保证后续off->on重置安全。 */
    td_init(&ladrc->td, td_r > 0.0f ? td_r : 0.0f, dt, td_n, td_max_x2);
    ladrc->use_td = td_r > 0.0f;
    if (!ladrc_configuration_valid(ladrc)) {
        ladrc_safe_state(ladrc);
    }
}

/** LADRC参数重置；TD每次完整初始化并重置状态。 */
void ladrc_reset(ladrc_t *ladrc, float beta1, float beta2, float beta3,
                 float kp, float kd, float b,
                 float dt, float k_aw,
                 float td_r, float td_n, float td_max_x2) {
    if (ladrc == NULL) {
        return;
    }
    if (!ladrc_arguments_valid(ladrc->max_output, beta1, beta2, beta3,
                               kp, kd, b, dt, k_aw, td_r, td_n, td_max_x2)) {
        ladrc_safe_state(ladrc);
        return;
    }

    ladrc->beta1 = beta1;
    ladrc->beta2 = beta2;
    ladrc->beta3 = beta3;
    ladrc->kp = kp;
    ladrc->kd = kd;
    ladrc->b = b;
    ladrc->dt = dt;
    ladrc->k_aw = k_aw; /* 兼容字段，当前未启用 */
    ladrc->x1 = 0.0f;
    ladrc->x2 = 0.0f;
    ladrc->x3 = 0.0f;
    ladrc->pre_out = 0.0f;
    ladrc->out = 0.0f;
    td_init(&ladrc->td, td_r > 0.0f ? td_r : 0.0f, dt, td_n, td_max_x2);
    ladrc->use_td = td_r > 0.0f;
    if (!ladrc_configuration_valid(ladrc)) {
        ladrc_safe_state(ladrc);
    }
}

/**
 * @brief LADRC 计算
 *
 * @note 三阶LESO采用前向欧拉积分，使用上一拍实际限幅输出pre_out；
 *       本拍输出限幅后才更新pre_out。固定dt不能消除实际调度抖动。
 */
float ladrc_calc(ladrc_t *ladrc, float target, float measure) {
    if (ladrc == NULL) {
        return 0.0f;
    }
    if (!ladrc_configuration_valid(ladrc) || !isfinite(target) ||
        !isfinite(measure) || !isfinite(ladrc->pre_out) ||
        !isfinite(ladrc->x1) || !isfinite(ladrc->x2) || !isfinite(ladrc->x3)) {
        return ladrc_safe_output(ladrc);
    }

    float td_target = ladrc->use_td ? td_update(&ladrc->td, target) : target;
    float td_velocity = ladrc->use_td ? ladrc->td.x2 : 0.0f;
    if (!isfinite(td_target) || !isfinite(td_velocity)) {
        return ladrc_safe_output(ladrc);
    }

    /* 三阶LESO：b*pre_out使用上一拍已施加的实际限幅输出。 */
    float error = measure - ladrc->x1;
    float dx1 = ladrc->x2 + ladrc->beta1 * error;
    float dx2 = ladrc->x3 + ladrc->b * ladrc->pre_out + ladrc->beta2 * error;
    float dx3 = ladrc->beta3 * error;
    float new_x1 = ladrc->x1 + dx1 * ladrc->dt; /* 前向欧拉 */
    float new_x2 = ladrc->x2 + dx2 * ladrc->dt; /* 前向欧拉 */
    float new_x3 = ladrc->x3 + dx3 * ladrc->dt; /* 前向欧拉 */
    if (!isfinite(new_x1) || !isfinite(new_x2) || !isfinite(new_x3)) {
        return ladrc_safe_output(ladrc);
    }
    ladrc->x1 = new_x1;
    ladrc->x2 = new_x2;
    ladrc->x3 = new_x3;

    float u0 = ladrc->kp * (td_target - ladrc->x1) +
               ladrc->kd * (td_velocity - ladrc->x2);
    /* k_aw回算暂时禁用：保留字段/参数仅为源码兼容，不参与控制律。 */
    float out_temp = 0.0f;
    if (isfinite(ladrc->b) && ladrc->b != 0.0f) {
        out_temp = (u0 - ladrc->x3) / ladrc->b;
    }
    abs_limit(&out_temp, ladrc->max_output);

    ladrc->out = out_temp;
    ladrc->pre_out = out_temp;
    return ladrc->out;
}
