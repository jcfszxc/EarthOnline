"""
CPU压力测试程序
目标：将 CPU 使用率维持在约 90%
原理：多进程忙等 + 动态占空比调节
"""

import multiprocessing
import time
import os
import psutil

TARGET_USAGE = 90.0   # 目标 CPU 使用率 (%)
CHECK_INTERVAL = 0.5  # 调节周期 (秒)
DUTY_INIT = 0.9       # 初始占空比


def worker(duty_ratio: multiprocessing.Value, stop_flag: multiprocessing.Event):
    """单核 worker：按占空比做忙等/休眠"""
    while not stop_flag.is_set():
        d = duty_ratio.value
        busy = CHECK_INTERVAL * d
        idle = CHECK_INTERVAL * (1.0 - d)
        deadline = time.perf_counter() + busy
        while time.perf_counter() < deadline:
            pass  # 忙等
        if idle > 0:
            time.sleep(idle)


def monitor(duty_ratio: multiprocessing.Value, stop_flag: multiprocessing.Event,
            n_workers: int):
    """主进程：监控 CPU 并动态调节占空比"""
    print(f"启动 {n_workers} 个 worker 进程，目标 CPU 使用率：{TARGET_USAGE}%")
    print("按 Ctrl+C 停止\n")
    print(f"{'时间':>8}  {'CPU%':>6}  {'占空比':>7}")
    print("-" * 28)

    try:
        while True:
            time.sleep(CHECK_INTERVAL)
            cpu = psutil.cpu_percent(interval=None)

            # 简单比例调节
            err = TARGET_USAGE - cpu
            duty_ratio.value = max(0.1, min(1.0, duty_ratio.value + err * 0.01))

            ts = time.strftime("%H:%M:%S")
            print(f"{ts:>8}  {cpu:>5.1f}%  {duty_ratio.value:>6.2f}")
    except KeyboardInterrupt:
        print("\n收到中断，正在停止所有进程...")
        stop_flag.set()


def main():
    n_cores = multiprocessing.cpu_count()
    print(f"检测到 {n_cores} 个逻辑核心")

    duty_ratio = multiprocessing.Value("d", DUTY_INIT)
    stop_flag = multiprocessing.Event()

    workers = []
    for _ in range(n_cores):
        p = multiprocessing.Process(target=worker, args=(duty_ratio, stop_flag), daemon=True)
        p.start()
        workers.append(p)

    # 预热一次 cpu_percent（首次调用返回 0）
    psutil.cpu_percent(interval=None)

    monitor(duty_ratio, stop_flag, n_cores)

    for p in workers:
        p.join(timeout=2)
    print("所有进程已退出。")


if __name__ == "__main__":
    main()
