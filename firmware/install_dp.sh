#!/bin/bash

# install_dp.sh
# Script cài đặt st-flash và ARM GCC toolchain

set -e

echo "===== Cập nhật hệ thống ====="
sudo apt update
sudo apt upgrade -y

echo "===== Cài đặt ARM GCC toolchain ====="
sudo apt install -y gcc-arm-none-eabi gdb-arm-none-eabi binutils-arm-none-eabi make

echo "===== Cài đặt st-flash (ST-Link) ====="
sudo apt install -y stlink-tools

echo "===== Kiểm tra phiên bản ====="
echo "ARM GCC version:"
arm-none-eabi-gcc --version

echo "st-flash version:"
st-flash --version

echo "===== Hoàn tất cài đặt ====="
