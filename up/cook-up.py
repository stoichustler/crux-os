from argparse import ArgumentParser
from os import system, environ
from pathlib import Path

SUPPORTED_CC_PREFIX = (
    "aarch64-none-linux-gnu-",
    "aarch64-linux-gnu-",
)

# QEMU
def qemu() -> None:
    ret = system(
            """
            qemu-system-aarch64 -machine virt,gic-version=3,virtualization=true \
            -cpu cortex-a57 -nographic -smp 8 -m 512M -bios u-boot.bin
            """)
    assert ret == 0

def clean() -> None:
    ret = system(f'make clean')
    assert ret == 0

def distclean() -> None:
    ret = system(f'make distclean')
    assert ret == 0

def kconfig(config: str) -> None:
    ret = system(f'make {config}')
    assert ret == 0

def build(toolchain: str) -> None:
    toolchain_prefix = ""

    for cc_prefix in SUPPORTED_CC_PREFIX:
        toolchain_gcc = f"{toolchain}/{cc_prefix}gcc"
        cc_gcc = Path(toolchain_gcc)
        if cc_gcc.exists():
            toolchain_prefix = f"{toolchain}/{cc_prefix}"

    if toolchain_prefix is None:
        raise Exception(f"{toolchain_prefix} not exists.")

    ret = system(f'make ARCH=arm CROSS_COMPILE={toolchain_prefix} -j32')
    assert ret == 0

def main() -> None:
    parser = ArgumentParser()
    parser.add_argument("-b", "--build", action="store_true", help="build u-boot")
    parser.add_argument("-c", "--clean", action="store_true", help="clean u-boot")
    parser.add_argument("-r", "--distclean", action="store_true", help="distclean u-boot")
    parser.add_argument("-q", "--qemu", action="store_true", help="run u-boot on qemu aarch64")
    parser.add_argument("-t", "--toolchain", metavar="TOOLCHAIN PATH", help="cross-compile toolchain path.")
    parser.add_argument("-f", "--config", metavar="CONFIG FILE", help="config file under configs/")

    args = parser.parse_args()

    toolchain = args.toolchain
    config = args.config

    if config is not None:
        kconfig(config)

    if args.qemu:
        qemu()

    if args.build:
        build(toolchain)

    if args.clean:
        clean()

    if args.distclean:
        distclean()


if __name__ == "__main__":
    main()
