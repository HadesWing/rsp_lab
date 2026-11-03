# bsp_lab —— 四个 BSP 教程（T1 模块 / T2 设备树 / T3 platform driver / T4 I2C）

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

### T2 实测记录（重启后，驱动未加载）

```
$ ls -d /proc/device-tree/bsp_demo
/proc/device-tree/bsp_demo
$ cat /proc/device-tree/bsp_demo/compatible | tr -d '\0'
hades,demo                                   # overlay 内容已合并进运行时设备树
$ cat /proc/device-tree/bsp_demo/label | tr -d '\0'
bsp-lab
$ dmesg | grep bspdemo
（空）                                        # 没有任何驱动认领它

$ ls /sys/bus/platform/devices/ | grep bsp_demo
bsp_demo                                     # 设备对象已在总线上登记，只是没有驱动
$ ls -l /dev/bspdemo
No such file or directory                    # 没有 probe → 没有设备节点
```

**一个反直觉的关键事实**：设备树里有 `compatible` 的节点，内核在启动时（`of_platform_populate`）就会把它登记成一个 `platform_device` 挂到 platform 总线上——所以设备**存在**，只是没有驱动和它匹配。

这解释了 probe 的真实触发条件：**不是"设备出现"触发 probe，而是"设备与驱动同时存在且 compatible 匹配"才触发**。先在总线上放设备、后加载驱动，probe 在加载那一刻才被调用（见 T3）。

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

### T3 实测记录

```
$ sudo insmod /tmp/demo_drv.ko                  # exit=0
[222.518366] bspdemo: probe 被调用（设备树 compatible 匹配成功）
                                                ← 加载这一刻，probe 才被调用

$ ls -l /dev/bspdemo
crw-rw-rw- 1 root root 10, 263                  # 主设备号 10 = misc，次设备号动态分配
$ cat /sys/class/misc/bspdemo/msg
hello from bsp_lab                              # sysfs 属性可读

$ echo "你好BSP，这是用户态写入" | sudo tee /dev/bspdemo
[223.050379] bspdemo: 用户写入 34 字节           # copy_from_user 成功
$ cat /dev/bspdemo
你好BSP，这是用户态写入                          # 回读一致，copy_to_user 成功

$ ls -l /sys/bus/platform/devices/bsp_demo/driver
... -> ../../../bus/platform/drivers/bspdemo    # 设备与驱动的绑定关系可查

$ sudo rmmod demo_drv
[229.195156] bspdemo: remove                    # remove 回调被调用
$ ls -l /dev/bspdemo                            # No such file
$ ls /sys/bus/platform/devices/bsp_demo/driver  # No such file（绑定解除）
$ ls -d /proc/device-tree/bsp_demo              # 仍在——设备树节点不属于驱动
```

**最值得记住的一对事实**：`rmmod` 之后 `/dev/bspdemo` 和驱动绑定都消失，但 `/proc/device-tree/bsp_demo` **依然存在**。因为设备树节点是内核启动时从 dtb 解出来的，不归任何驱动所有；驱动只是"解释并使用"它，不拥有它。这正对应前面 T2 的状态——设备永远在，驱动来了又走。

❌ 如果 insmod 成功但没有 probe 日志 → compatible 没匹配上（大小写/拼写/overlay 没生效）
❌ 如果 insmod 报 `Invalid argument` 或 `version magic` → 模块不是用同一棵内核树编的

## T4 I2C：用假芯片练真实的 I2C 驱动（不需要任何器件）

器件还没到货也能练。内核自带 `i2c-stub`：它在软件里模拟一个 I2C 适配器 + 若干个 256 寄存器的假芯片，读写走真实的 SMBus 协议栈，只是最终数据落进内存而不是线上。

**编出来不用改配置**：`i2c-stub.c` 没有任何 `CONFIG` 守卫、只用导出符号，所以 build.sh 用命令行覆盖变量单独编它：

```bash
make -C ~/linux M=~/linux/drivers/i2c CONFIG_I2C_STUB=m modules
```

这条命令本身就是个考点——**证明"某个驱动能不能编"取决于源码是否依赖 CONFIG 守卫，而不是它有没有进 .config**。

### 步骤（不用重启）

```bash
sudo modprobe i2c-dev                                   # 生成 /dev/i2c-*
sudo insmod /tmp/i2c-stub.ko chip_addr=0x50             # 假芯片上线
sudo i2cdetect -l                                       # 看新适配器编号 N
sudo i2cdetect -y N                                     # 0x50 位应显示 50
```

用户态验证寄存器读写（这一步走的是 `i2c-dev` 的 ioctl，不涉及我们的驱动）：

```bash
sudo i2cset -y N 0x50 0x00 0x5a
sudo i2cget -y N 0x50 0x00        # → 0x5a
```

装驱动、实例化设备、读寄存器：

```bash
sudo insmod /tmp/demo_i2c.ko
echo demo_i2c 0x50 | sudo tee /sys/bus/i2c/devices/i2c-N/new_device
dmesg | tail -5                   # probe 被调用 + 寄存器初值 0x5a
cat /dev/demo_i2c                 # → reg[0x00]=0x5a
printf '\x77' | sudo tee /dev/demo_i2c
cat /dev/demo_i2c                 # → reg[0x00]=0x77
sudo rmmod demo_i2c               # remove：设备已注销
```

清理：`sudo rmmod i2c-stub`（适配器一起消失）。

### 这一步的四个考点

| 现象 | 考点 |
|---|---|
| `new_device` 一写就 probe | **I2C 设备有三种实例化方式**：设备树、`new_device` sysfs、板级 `i2c_board_info`。产品里用第一种，调试时第二种最方便 |
| probe 里 `i2c_smbus_read_byte_data` 返回值必须判负 | `-ENXIO` = 地址上没器件应答，`-EREMOTEIO` = 应答但数据不全。**不判负是新手最常犯的错**，会让 probe 带着坏状态继续跑 |
| `/dev/demo_i2c` 读写 | `copy_to_user/copy_from_user` 不能换成 `memcpy`（用户地址可能换页、可能非法） |
| `rmmod` 之后 `new_device` 的 client 还在 | client 是总线的孩子，不归驱动所有——**和 T3 的设备树节点同一个道理** |

### T4 实测记录（2026-11-03）

**第一个坑：`insmod` 不带参数直接失败**（第一次跑就是在这一步卡住的）：

```
i2c-stub: Please specify a chip address
insmod: ERROR: could not insert module /tmp/i2c-stub.ko: No such device
```

根因在 `i2c_stub_init()` 开头：`if (!chip_addr[0]) { pr_err("Please specify a chip address"); return -ENODEV; }`
——`chip_addr` 是 `module_param_array` 参数，没给值时 `chip_addr[0]` 为 0，模块 init 直接返回 `-ENODEV`，insmod 就报 "No such device"。**必须写成 `chip_addr=0x50`**（大小写不敏感，0x03–0x77 之间）。

带参数后完整闭环（本机实测）：

```
$ sudo insmod /tmp/i2c-stub.ko chip_addr=0x50
i2c-stub: Virtual chip at 0x50

$ sudo i2cdetect -l
i2c-11  smbus  SMBus stub driver  SMBus adapter      ← 编号不是固定 1，每次装可能变

$ sudo i2cdetect -y 11
50: 50 -- -- ...

$ sudo i2cset -y 11 0x50 0x00 0x5a && sudo i2cget -y 11 0x50 0x00
0x5a                                                  ← 用户态走 i2c-dev ioctl

$ sudo insmod /tmp/demo_i2c.ko
$ echo demo_i2c 0x50 | sudo tee /sys/bus/i2c/devices/i2c-11/new_device
demo_i2c 11-0050: probe 被调用：地址 0x50，适配器 SMBus stub driver
demo_i2c 11-0050: 寄存器 0x00 初值 = 0x5a
demo_i2c 11-0050: 就绪：/dev/demo_i2c

$ ls -l /dev/demo_i2c
crw------- 1 root root 10, 263                          ← misc 设备：主 10，次号动态分配

$ sudo cat /dev/demo_i2c
reg[0x00]=0x5a
$ sudo sh -c "printf 'w' > /dev/demo_i2c"               ← 写 0x77
demo_i2c 11-0050: 写 reg[0x00] = 0x77
$ sudo cat /dev/demo_i2c
reg[0x00]=0x77

$ sudo i2cdetect -y 11
50: UU -- -- ...                                        ← UU 表示已被驱动占用（不再探测）
```

**反向实验（验证"返回值必须判负"是真的有用）**：往没有器件的 0x51 实例化——

```
demo_i2c 11-0051: probe 被调用：地址 0x51，适配器 SMBus stub driver
demo_i2c 11-0051: 探测失败（器件无应答）: -19          ← -19 = -ENXIO
```

probe 返回负值 → 驱动**没有**绑定成功、`/dev/demo_i2c` 没被创建。如果不判负，这里会带着野指针继续 `misc_register`，用户态一读就是一堆 I/O error。

**两个容易踩的细节**：
- `printf '\x77' > /dev/demo_i2c` 在嵌套 ssh/引号里会被二次转义吃掉反斜杠（实测写进去变成 `0x5c`），改用字符 `w` 或 `echo -n` 更稳
- 设备节点权限是 `crw-------`（root only），`sudo cat` 才读得到

### 换成真实器件要改什么

只改三处：`DEMO_I2C_ADDR` 换成器件手册里的地址、寄存器号换成它的 ID 寄存器、以及用它的真实时序（有的芯片要先写指针再读，有的要连读多字节）。**骨架不变**——这就是为什么先用假芯片练是划算的：骨架通了，换器件只剩查手册。

## T5 用户态裸寄存器：/dev/mem 直读外设（不买 STM32 也能练寄存器操作）

**目的**：证明"读写寄存器"与是否有 RTOS/裸机无关——mmap 物理地址后，操作语义与 STM32 的 `USART1->SR` 完全一致。

板上原生编译（源码 60 行，1 秒编完，无需交叉编译）：

```bash
ssh pi-lab 'cat > /tmp/regpeek.c' < regpeek/regpeek.c
ssh pi-lab 'gcc /tmp/regpeek.c -o /tmp/regpeek -Wall && sudo /tmp/regpeek'
```

### T5 实测记录（2026-11-03）

```
GPFSEL1 = 0x00024000
  GPIO14 功能字段 = 4   (4 = ALT0 = TXD0)
  GPIO15 功能字段 = 4   (4 = ALT0 = RXD0)
UART0_FR = 0x00000091
  TXFE=1 (TX FIFO 空)  RXFE=1 (RX FIFO 空)  BUSY=0
```

判读：
- `0x24000` 的 bit14:12=100、bit17:15=100，**与 pinctrl-bcm2835.c 的 `FSEL_REG/FSEL_SHIFT` 公式推算完全一致**（14 号脚 = 基址+0x4 的 bit12..14）——设备树里 `brcm,function=<4>` 写进硬件后就是这个值
- `FR=0x91` = CTS=1（对端允许发送）+ TXFE + RXFE：串口硬件的实时状态，用户态直接摸到了

与 STM32 的等价关系：

| 这里的代码 | STM32 等价物 |
|---|---|
| `gpio[GPFSEL1_IDX]` 读功能选择 | 读 `GPIOA->CRL` |
| `uart[UART_FR_IDX]` 读标志寄存器 | 读 `USART1->SR`（等 TXE 位） |

**注意**：本实验只读。写 GPFSEL 前必须想清楚——写错一个 bit 会把串口弄丢。

## 撤销（把板子恢复原样）

```bash
ssh pi-lab 'sudo sed -i "/dtoverlay=demo-overlay/d" /boot/firmware/config.txt'
ssh pi-lab 'sudo rm -f /boot/firmware/overlays/demo-overlay.dtbo'
ssh pi-lab 'sudo reboot'
```
