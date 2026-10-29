#!/usr/bin/env bash
# 把编好的 .ko 和 .dtbo 推到板子 /tmp（走 ssh 管道，不用 scp）
# 用法：bash push.sh
set -uo pipefail
cd "$(dirname "$0")"

SSH_EXE=""
for c in "/mnt/c/Windows/System32/OpenSSH/ssh.exe" "ssh.exe"; do
  if command -v "$c" >/dev/null 2>&1 || [ -x "$c" ]; then SSH_EXE="$c"; break; fi
done
[ -n "$SSH_EXE" ] || { echo "[中止] 找不到 Windows OpenSSH"; exit 1; }

push_one() {
  local src=$1 dst=$2
  [ -f "$src" ] || { echo "[跳过] 没有 $src（先跑 build.sh）"; return 0; }
  "$SSH_EXE" pi-lab "cat > '$dst'" < "$src" || { echo "[失败] $src"; return 1; }
  echo "  ✓ $src → pi-lab:$dst"
}

push_one hello_mod/hello_mod.ko   /tmp/hello_mod.ko
push_one demo_drv/demo_drv.ko     /tmp/demo_drv.ko
push_one dts/demo-overlay.dtbo    /tmp/demo-overlay.dtbo
push_one i2c_demo/i2c-stub.ko     /tmp/i2c-stub.ko
push_one i2c_demo/demo_i2c.ko     /tmp/demo_i2c.ko

echo
echo "T4（不用重启，不用任何器件）："
echo "  ssh pi-lab 'sudo modprobe i2c-dev'"                        # 生成 /dev/i2c-*
echo "  ssh pi-lab 'sudo insmod /tmp/i2c-stub.ko chip_addr=0x50'"   # 假芯片上线
echo "  ssh pi-lab 'sudo i2cdetect -l'"                            # 找出 stub 的适配器号
echo "  ssh pi-lab 'sudo i2cdetect -y <N>'"                        # 0x50 应显示 UU/50
echo "  ssh pi-lab 'sudo i2cset -y <N> 0x50 0x00 0x5a && sudo i2cget -y <N> 0x50 0x00'"
echo "  ssh pi-lab 'sudo insmod /tmp/demo_i2c.ko && dmesg | tail -5'"
echo "  ssh pi-lab 'echo demo_i2c 0x50 | sudo tee /sys/bus/i2c/devices/i2c-<N>/new_device'"
echo "  ssh pi-lab 'cat /dev/demo_i2c; printf \"\\\\x77\" | sudo tee /dev/demo_i2c; cat /dev/demo_i2c'"
echo
echo "其他教程："
echo "  T1（不用重启）："
echo "    ssh pi-lab 'sudo insmod /tmp/hello_mod.ko && dmesg | tail -3'"
echo
echo "  T2/T3（要装 overlay 并重启）："
echo "    ssh pi-lab 'sudo cp /tmp/demo-overlay.dtbo /boot/firmware/overlays/ \\"
echo "                 && echo dtoverlay=demo-overlay | sudo tee -a /boot/firmware/config.txt'"
echo "    ssh pi-lab 'sudo reboot'"
