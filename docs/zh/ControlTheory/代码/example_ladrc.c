#include "ladrc.h"
#include <stdio.h>

int main(void) {
    const float h = 0.001f;
    const float reference = 1.0f;
    float theta = 0.0f;
    float omega = 0.0f;
    ladrc_t controller;

    /* 与教程数值例一致：b0=5，wc=10，wo=40，电流限制为 ±2 A。 */
    ladrc_init(&controller,
               2.0f,                 /* max_output */
               120.0f, 4800.0f, 64000.0f,
               100.0f, 20.0f, 5.0f,
               h,
               0.0f,                 /* k_aw 当前保留但未启用 */
               0.0f, 3.0f, 0.0f);   /* 禁用 TD */

    printf("t,theta,omega,u,z3\n");
    for (int k = 0; k <= 5000; ++k) {
        float t = k * h;
        float load = t >= 2.0f ? -4.0f : 0.0f;
        float u = ladrc_calc(&controller, reference, theta);

        if (k % 250 == 0) {
            printf("%.3f,%.6f,%.6f,%.6f,%.6f\n",
                   t, theta, omega, u, ladrc_get_disturbance(&controller));
        }

        /* 同一拍内将限幅后的 u 施加到二阶电机模型，再推进对象状态。 */
        float theta_next = theta + h * omega;
        float omega_next = omega + h * (-2.0f * omega + 5.0f * u + load);
        theta = theta_next;
        omega = omega_next;
    }
    return 0;
}
