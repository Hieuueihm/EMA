#!/bin/bash

# install_dp.sh
# Script cài đặt st-flash và ARM GCC toolchain

set -e

echo "===== System update====="
sudo apt update
sudo apt upgrade -y

echo "===== ARM gcc tool-chain ====="
sudo apt install -y gcc-arm-none-eabi gdb-arm-none-eabi binutils-arm-none-eabi make

echo "===== st-flash (ST-Link) ====="
sudo apt install -y stlink-tools

echo "===== Check the version ====="
echo "ARM GCC version:"
arm-none-eabi-gcc --version

echo "st-flash version:"
st-flash --version

echo "===== Done ====="
