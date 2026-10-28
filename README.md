# bsp_lab —— 今天的三个 BSP 教程

配套文档：《BSP能力盘点_已踩与未涉及_含今日教程》（本地工作区文档，不入库）

```
bsp_lab/
├── hello_mod/    T1  第一个内核模块（20 分钟，不用重启）
├── dts/          T2  设备树 overlay（30 分钟，需重启一次）
├── demo_drv/     T3  platform driver 匹配设备树（60 分钟，BSP 面试核心）
├── build.sh      一键编译（模块 + overlay）
└── push.sh       推到板子 /tmp
```

## 0. 编译

```bash
cd ~/bsp_lab
bash build.sh
```

成功标志：末尾列出三个产物，且反编译对照里能看到 `compatible = "hades,demo"`。

## T1 第一个内核模块（先做这个，最快见效）

```bash
bash push.sh
ssh pi-lab 'sudo insmod /tmp/hello_mod.ko; dmesg | tail -3'
ssh pi-lab 'sudo rmmod hello_mod; dmesg | tail -2'
```

✅ 判据：`dmesg` 里出现 `hello_mod: 加载成功，jiffies=...`，卸载后出现 `hello_mod: 卸载`。

### T1 实测记录（Raspberry Pi 1 B+ / 6.18.59-R1 / ARMv6）

模块产物：`vermagic=6.18.59-R1 mod_unload modversions ARMv6 p2v8`，cross-compiled，5012 字节。

```
$ sudo insmod /tmp/hello_mod.ko      # exit=0
[75940.954235] hello_mod: loading out-of-tree module taints kernel.
[75940.954235] hello_mod: 加载成功，jiffies=7564032

$ lsmod | grep hello_mod
hello_mod              12288  0        # 12KB 常驻内存，引用计数 0
$ cat /sys/module/hello_mod/initstate
live

$ sudo rmmod hello_mod               # exit=0
[75941.482517] hello_mod: 卸载
$ ls /sys/module/hello_mod
No such file or directory            # 内核态痕迹彻底清除
```

两条值得记的现象：

- `jiffies/100 ≈ 系统 uptime 秒数`（7564032/100 = 75640s），证明模块确实在内核态读到了全局节拍计数器，而不是用户态打印。
- `loading out-of-tree module taints kernel` 不是错误：树外模块会在内核 taint 标记上加位，用于日后 oops 归因——这正是 `M=` 外部模块的正常身份。

## T2 设备树：装 overlay

```bash
bash push.sh     # 已经把 dtbo 推到 /tmp
ssh pi-lab 'sudo cp /boot/firmware/config.txt /boot/firmware/config.txt.bak'
ssh pi-lab 'sudo cp /tmp/demo-overlay.dtbo /boot/firmware/overlays/'
ssh pi-lab 'echo dtoverlay=demo-overlay | sudo tee -a /boot/firmware/config.txt'
ssh pi-lab 'sudo reboot'
```

等板子回来（约 3 分钟，启动慢是已知问题，见盘点文档）：

```bash
ssh pi-lab 'ls /proc/device-tree/ | grep -i bsp_demo'      # 出现 bsp_demo 节点
ssh pi-lab 'cat /proc/device-tree/bsp_demo/compatible'     # 打印 hades,demo
ssh pi-lab 'dmesg | grep -i "bspdemo: probe"'              # 此时还没有驱动，应该没有输出
```

✅ 判据：`/proc/device-tree/bsp_demo` 存在（设备树生效），但驱动还没写，所以没有 probe 日志。
**这一对"节点有了、驱动还没接"正是理解设备树和驱动分家的关键。**

## T3 platform driver 接上设备树

```bash
ssh pi-lab 'sudo insmod /tmp/demo_drv.ko; dmesg | tail -5'
ssh pi-lab 'ls -l /dev/bspdemo'
ssh pi-lab 'cat /sys/class/misc/bspdemo/msg'
ssh pi-lab 'echo 你好BSP | sudo tee /dev/bspdemo; cat /dev/bspdemo'
ssh pi-lab 'sudo rmmod demo_drv; dmesg | tail -2'
```

✅ 判据：
- `dmesg` 出现 `bspdemo: probe 被调用（设备树 compatible 匹配成功）` ← **唯一判据**
- `/dev/bspdemo` 存在，能读写，回读内容与写入一致
- `/sys/class/misc/bspdemo/msg` 能读到当前内容
- `rmmod` 后 `remove` 日志出现、`/dev/bspdemo` 消失

❌ 如果 insmod 成功但没有 probe 日志 → compatible 没匹配上（大小写/拼写/overlay 没生效）
❌ 如果 insmod 报 `Invalid argument` 或 `version magic` → 模块不是用同一棵内核树编的

## 撤销（把板子恢复原样）

```bash
ssh pi-lab 'sudo sed -i "/dtoverlay=demo-overlay/d" /boot/firmware/config.txt'
ssh pi-lab 'sudo rm -f /boot/firmware/overlays/demo-overlay.dtbo'
ssh pi-lab 'sudo reboot'
```
