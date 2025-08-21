#!/usr/bin/env python3

#######################################################
#                      _____     __  __
#             /\/\/\/\/ __  \/\ / _\/__\
#             \ - \ \ \ \/ // // _\/  \
#              \/\/\_/\_/\/ \__\__\\/\/ @2025
#
#                  - Hustle Embedded -
#######################################################

from argparse import ArgumentParser
from os import system, environ
from pathlib import Path

OUTPUT = "build"

SUPPORTED_CC_PREFIX = (
    "aarch64-none-linux-gnu-",
    "aarch64-linux-gnu-",
)

# QEMU
def qemu() -> None:
    ret = system(
            f"""
    qemu-system-aarch64 -machine virt,gic-version=3,virtualization=true \
        -cpu cortex-a57 -nographic -smp 8 -m 512M -bios {OUTPUT}/up.bin
            """)
    assert ret == 0

def clean() -> None:
    ret = system(f'make clean O={OUTPUT}')
    assert ret == 0

def remove() -> None:
    ret = system(f'rm -rf {OUTPUT}')
    assert ret == 0

def kconfig(config: str) -> None:
    config_file = Path(config).name
    print(f"### using {config_file}\n")
    ret = system(f'make {config_file} O={OUTPUT}')
    assert ret == 0

def list_config_files() -> None:
    ret = system("find ./configs -name '*defconfig'")
    assert ret == 0

def build(toolchain: str) -> None:
    toolchain_prefix = ""

    if toolchain is None:
        raise Exception("path of cross-compile toolchain is missing.")

    for cc_prefix in SUPPORTED_CC_PREFIX:
        toolchain_gcc = f"{toolchain}/{cc_prefix}gcc"
        cc_gcc = Path(toolchain_gcc)
        if cc_gcc.exists():
            toolchain_prefix = f"{toolchain}/{cc_prefix}"

    ret = system(f'make ARCH=arm CROSS_COMPILE={toolchain_prefix} O={OUTPUT} -j32')
    assert ret == 0

def main() -> None:
    parser = ArgumentParser()
    parser.add_argument("-b", "--build", action="store_true", help="build u-boot")
    parser.add_argument("-c", "--clean", action="store_true", help="clean u-boot")
    parser.add_argument("-l", "--list", action="store_true", help="list config files")
    parser.add_argument("-r", "--remove", action="store_true", help="remove u-boot")
    parser.add_argument("-q", "--qemu", action="store_true", help="run u-boot on qemu aarch64")
    parser.add_argument("-t", "--toolchain", metavar="TOOLCHAIN PATH", help="cross-compile toolchain path.")
    parser.add_argument("-f", "--config", metavar="CONFIG FILE", help="config file under configs/")

    args = parser.parse_args()

    toolchain = args.toolchain
    config = args.config

    if config is not None:
        kconfig(config)

    if args.list:
        list_config_files()

    if args.qemu:
        qemu()

    if args.build:
        build(toolchain)

    if args.clean:
        clean()

    if args.remove:
        remove()


if __name__ == "__main__":
    main()
