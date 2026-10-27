#!/usr/bin/env bash
# 一键编译：内核模块（hello_mod / demo_drv）+ 设备树 overlay
# 用法：bash build.sh
set -euo pipefail
cd "$(dirname "$0")"

KDIR=${KDIR:-$HOME/linux}

# ---------- 前置检查（失败原因写清楚，别让人猜）----------
[ -f "$KDIR/Makefile" ] || { echo "[中止] 找不到内核树：$KDIR"; exit 1; }
[ -f "$KDIR/Module.symvers" ] || {
  echo "[中止] $KDIR/Module.symvers 不存在，内核树还没完整编译过。"
  echo "        先在 $KDIR 跑一次：make -j8 ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- zImage modules"
  exit 1; }
command -v arm-linux-gnueabihf-gcc >/dev/null || { echo "[中止] 缺交叉编译器"; exit 1; }
command -v dtc >/dev/null || { echo "[中止] 缺 dtc：sudo apt install -y device-tree-compiler"; exit 1; }

# ---------- 内核模块 ----------
echo "==> 编译内核模块（KDIR=$KDIR）"
make -C "$KDIR" M="$PWD" ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- modules

# ---------- 设备树 overlay ----------
echo
echo "==> 编译设备树 overlay"
dtc -@ -I dts -O dtb -o dts/demo-overlay.dtbo dts/demo-overlay.dts

# ---------- 反编译对照（教学用：看 dtbo 里到底写了什么）----------
echo
echo "==> 反编译回来核对（-O dts 把二进制又变回文本）"
dtc -I dtb -O dts dts/demo-overlay.dtbo 2>/dev/null | grep -A4 "bsp_demo"

echo
echo "==> 产物"
ls -lh hello_mod/hello_mod.ko demo_drv/demo_drv.ko dts/demo-overlay.dtbo
echo
echo "装到板上：bash push.sh"
