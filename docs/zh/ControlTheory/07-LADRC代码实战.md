# LADRC 代码实战

> **最近修改日期**：2026-11-04
>
> **参与者**：Jackrainman
>
> **前置知识**：[LADRC算法详解](./06-LADRC算法详解.md)

这份实现包含一阶 LADRC、二阶 LADRC 和可选的跟踪微分器（TD）。完整源码不再复制到正文，直接看仓库文件：

- [ladrc.h](代码/ladrc.h)：数据结构、初始化和调参接口
- [ladrc.c](代码/ladrc.c)：完整算法
- [example_ladrc.c](代码/example_ladrc.c)：可在电脑上运行的二阶电机例

代码使用连续带宽增益和前向欧拉更新，与上一篇公式一致。

## 1. 一阶和二阶接口

一阶对象使用二阶 LESO，扰动估计是 `x2`：

$$
\dot y=f+b_0u,\qquad u=\frac{k_p(r-z_1)-z_2}{b_0}
$$

二阶对象使用三阶 LESO，扰动估计是 `x3`：

$$
\ddot y=f+b_0u,\qquad
u=\frac{k_p(v_1-z_1)+k_d(v_2-z_2)-z_3}{b_0}
$$

对应接口如下。

```c
first_order_ladrc_t first;
ladrc_t second;

first_order_ladrc_init(&first, max_output,
                       beta1, beta2, kp, b0, dt,
                       0.0f, td_r, td_n, td_max_x2);

ladrc_init(&second, max_output,
           beta1, beta2, beta3, kp, kd, b0, dt,
           0.0f, td_r, td_n, td_max_x2);

float u1 = first_order_ladrc_calc(&first, target, measure);
float u2 = ladrc_calc(&second, target, measure);
```

`b0` 必须非零，`dt` 必须大于零，`max_output` 不能为负数。初始化参数非法时，当前实现会进入全零安全状态；初始化接口没有错误返回值，调用方仍应在初始化前检查参数，不要把安全清零当成完整的故障诊断。

## 2. 二阶控制的一拍计算

核心代码与公式的对应关系如下。为便于阅读，这里省略了完整源码中的非法值检查。

```c
float td_target = ladrc->use_td ? td_update(&ladrc->td, target) : target;
float td_velocity = ladrc->use_td ? ladrc->td.x2 : 0.0f;

/* 本拍 LESO 使用上一拍实际限幅输出 pre_out。 */
float error = measure - ladrc->x1;
float dx1 = ladrc->x2 + ladrc->beta1 * error;
float dx2 = ladrc->x3 + ladrc->b * ladrc->pre_out + ladrc->beta2 * error;
float dx3 = ladrc->beta3 * error;

float new_x1 = ladrc->x1 + ladrc->dt * dx1;
float new_x2 = ladrc->x2 + ladrc->dt * dx2;
float new_x3 = ladrc->x3 + ladrc->dt * dx3;
ladrc->x1 = new_x1;
ladrc->x2 = new_x2;
ladrc->x3 = new_x3;

float u0 = ladrc->kp * (td_target - ladrc->x1) +
           ladrc->kd * (td_velocity - ladrc->x2);
float out = (u0 - ladrc->x3) / ladrc->b;
abs_limit(&out, ladrc->max_output);
ladrc->pre_out = out;
```

实际执行顺序是：

1. 读取本拍目标与测量值；
2. 更新 TD（若启用）；
3. 用上一拍实际输出更新 LESO；
4. 计算本拍请求并限幅；
5. 把限幅后的输出保存到 `pre_out`，供下一拍 LESO 使用。

三个 LESO 导数都先根据旧状态计算，再一起更新。不要在更新 `x1` 后，又用新 `x1` 重新计算本拍误差和其余导数。

```mermaid
flowchart LR
    Y["读取 y_m[k]"] --> E["计算误差 e[k]"]
    UP["上一拍实际 u_app[k-1]"] --> E
    E --> Z["同步更新 z1、z2、z3"]
    Z --> U["计算 u0 与请求 u_req[k]"]
    U --> S["限幅得到 u_app[k]"]
    S --> P["施加到对象"]
    S --> SAVE["保存给下一拍 LESO"]
```

## 3. 用带宽法初始化

二阶对象的参数关系为

$$
k_p=\omega_c^2,\qquad k_d=2\omega_c
$$

$$
\beta_1=3\omega_o,\qquad
\beta_2=3\omega_o^2,\qquad
\beta_3=\omega_o^3
$$

沿用上一篇的电机例：$b_0=5$，$\omega_c=10$ rad/s，$\omega_o=40$ rad/s，$dt=0.001$ s，电流限制为 $\pm2$ A。

```c
ladrc_t motor;
ladrc_init(&motor,
           2.0f,                    // 最大电流
           120.0f, 4800.0f, 64000.0f,
           100.0f, 20.0f, 5.0f,
           0.001f,
           0.0f,                    // k_aw 当前不参与计算
           0.0f, 3.0f, 0.0f);       // 禁用 TD
```

也可以用调参接口从带宽重新计算增益：

```c
ladrc_tune(&motor, 40.0f, 10.0f, 5.0f);
```

`ladrc_tune()` 只写入增益，不检查采样稳定性。修改 `wc/wo/dt` 后，应重新检查 `wo*dt`、计算时延、噪声和输出饱和。

## 4. TD 的启用与单位

`td_r<=0` 表示禁用 TD，目标直接透传；`td_r>0` 表示启用。`td_n` 是无量纲滤波因子，内部使用 `h0=td_n*dt`。

TD 的 `r` 与目标加速度同量纲，不是“每秒计算次数”，也不能脱离目标单位固定推荐一个数。若目标幅值为 $A$，静止到静止的最速切换时间希望接近 $T$，可用

$$
r\approx\frac{4A}{T^2}
$$

作为初始估计，再根据噪声、速度和执行器限制下调或调整。

启用 TD 后，二阶控制器会同时使用 `td.x1` 和 `td.x2`，即参考位置与参考速度。禁用 TD 时参考速度取零，更适合常量或分段常量目标。

切换 TD 状态时，使用初始化或 reset 接口完整设置参数和状态：

```c
ladrc_reset(&motor,
            120.0f, 4800.0f, 64000.0f,
            100.0f, 20.0f, 5.0f,
            0.001f, 0.0f,
            20.0f, 3.0f, 0.0f);    // 启用 TD，并清空旧状态
```

reset 会清空 LESO 状态。若系统正在运行，需要从当前输出规划无扰启动，而不是直接把内部状态全部跳到零。

## 5. 饱和与非法输入

当前版本只采用两项基本措施：

- 输出经过 `max_output` 限幅；
- LESO 使用上一拍实际限幅输出，而不是未限幅请求。

旧版 `k_aw` 回算方向有问题，本版保留字段和参数以兼容接口，但**不参与控制律**。不要把它当成已经可用的抗饱和功能。

运行时会检查非有限目标、测量、状态和参数。检测到 NaN/Inf 或非法配置时输出零。实际项目还需要自行定义传感器失效、漏周期、驱动器饱和和急停策略。

## 6. 在 RTOS 中调用

`dt` 是名义控制周期。把它固定在结构体里，只能避免把测得的周期波动直接带入积分，不能消除真实调度抖动。

应由硬件定时器或绝对周期任务调用控制器，并保证计算、通信和输出时间小于控制周期。FreeRTOS 中可使用 `vTaskDelayUntil`，不要用“计算耗时 + `vTaskDelay(1)`”冒充固定 1 ms 周期。

在线调参时，先在任务外计算好一组参数，再在临界区或一致快照中更新，避免控制中断读到一半旧参数、一半新参数。

## 7. 编译并运行电机例

在仓库根目录执行：

```bash
mkdir -p .agents/.cache/ladrc
gcc -std=c99 -Wall -Wextra -Werror \
  -Idocs/zh/ControlTheory/代码 \
  docs/zh/ControlTheory/代码/example_ladrc.c \
  docs/zh/ControlTheory/代码/ladrc.c \
  -o .agents/.cache/ladrc/example_ladrc.exe -lm
.agents/.cache/ladrc/example_ladrc.exe
```

示例使用

$$
\ddot\theta=-2\dot\theta+5u+d
$$

在 2 s 时加入 $d=-4$ rad/s² 的负载扰动，并输出角度、速度、实际电流和扰动估计。初始大阶跃会触发电流饱和，因此可以同时观察限幅与扰动补偿，而不是只验证代码能否编译。

## 8. 已做的主机检查

测试源码位于 `.agents/tests/ladrc/test_ladrc.c`，使用 GCC C99 和 `-Wall -Wextra -Werror` 编译。测试覆盖：

- 一阶、二阶初始化与重复 reset；
- 非法 `b/dt/max_output`、NaN/Inf 目标和测量；
- TD 的启用、禁用、在线 `r=0` 与参考速度前馈；
- 正负饱和及 LESO 的上一拍实际输入时序；
- 非零扰动估计和带宽调参公式。

这些检查只证明主机实现符合当前公式与约定，不等于 MCU 或实车验证。调参步骤和仿真对比见 [《LADRC实战指南》](./08-LADRC实战指南.md)。
