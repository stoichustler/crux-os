# Copyright (c) 2025 HUSTLER.
# SPDX-License-Identifier: Apache-2.0

from argparse import ArgumentParser
from os import system, environ
from pathlib import Path

BUILD_DIR = "zeus/out"

SUPPORTED_PROJECTS = (
    "radxa_zero3w/rk3568/xen_dom0",
    "qemu_cortex_a53/qemu_cortex_a53",
)

SUPPORTED_CC_PREFIX = (
    "aarch64-none-elf-",
    "aarch64-zephyr-elf-",
)

def print_banner() -> None:
    print(
"  __  __      __\n" \
" / _\\/__\\/\\/\\/ _\\\n" \
"/ / /  \\/ / /\\ \\\n" \
"\\_\\ \\/\\/\\__/\\__/ CRUX 2025\n" \
    )

def clean() -> None:
    print("-- cleaning up all built objects ...\n")
    ret = system(f'rm -rf {BUILD_DIR}')
    assert ret == 0

def ctags() -> None:
    print("-- generating tags for code tracking ...\n")
    ret = system('ctags --languages=Asm,c,c++ -R')
    assert ret == 0

def scp(remote_ip: str, remote_user: str, remote_target: str) -> None:
    print(f'-- sending zephyr.bin to remote: {remote_user}@{remote_ip}\n')
    ret = system(f'scp {BUILD_DIR}/zephyr/zephyr.bin {remote_user}@{remote_ip}:{remote_target}')
    assert ret == 0

def man() -> None:
    print("""
This project requires python 3.12+

python3.12 -m pip install pykwalify
python3.12 -m pip install packaging
python3.12 -m pip install pyelftools
          """
    )

def enable_menuconfig() -> None:
    build_dir = Path(BUILD_DIR)
    build_makefile = build_dir / "Makefile"
    if build_makefile.exists():
        ret = system(f'cd {build_dir} && make menuconfig')
        assert ret == 0
    else:
        print("please build zeus first.")

def build_zephyr(
    project: str,
    toolchain_path: str,
) -> None:
    """
        build zeus
    """
    print("-- building zeus (a.k.a zephyr) ...\n")
    zephyr_base = Path.cwd()
    build_dir = Path(BUILD_DIR)
    build_dir.mkdir(exist_ok=True, parents=True)

    environ["ZEPHYR_BASE"] = f'{zephyr_base}'

    toolchain_prefix = ""

    for cc_prefix in SUPPORTED_CC_PREFIX:
        toolchain_gcc = f"{toolchain_path}/{cc_prefix}gcc"
        cc_gcc = Path(toolchain_gcc)
        if cc_gcc.exists():
            toolchain_prefix = f"{toolchain_path}/{cc_prefix}"

    if toolchain_prefix is None:
        raise Exception(f"{toolchain_prefix} not exists.")

    cmd = f'cd {build_dir} && cmake .. -DBOARD={project} -DCROSS_COMPILE={toolchain_prefix}'

    ret = system(cmd)
    assert ret == 0

    ret = system(f"cd {build_dir} && make")
    assert ret == 0


def main() -> None:
    parser = ArgumentParser()
    parser.add_argument("-b", "--build", metavar="PROJECT", help="board to build.")
    parser.add_argument("-t", "--toolchain", metavar="TOOLCHAIN PATH", help="cross-compile toolchain path.")
    parser.add_argument("-l", "--list", action="store_true", help="list supported projects.")
    parser.add_argument("-c", "--clean", action="store_true", help="remove build directory.")
    parser.add_argument("-g", "--ctags", action="store_true", help="create tags for code tracking")
    parser.add_argument("-m", "--man", action="store_true", help="show mannual")
    parser.add_argument("-u", "--menuconfig", action="store_true", help="enable menuconfig")
    # scp
    parser.add_argument("-s", "--scp", action="store_true", help="use scp")
    parser.add_argument("-i", "--ip", metavar="IP", help="remote board ip")
    parser.add_argument("-n", "--user", metavar="USER NAME", help="remote board user name")
    parser.add_argument("-o", "--rpath", metavar="TARGET PATH", help="remote board target path")

    args = parser.parse_args()

    r_ip = args.ip
    r_user = args.user
    r_path = args.rpath

    # personal interest, never mind.
    print_banner()

    if args.scp:
        scp(r_ip, r_user, r_path)

    if args.man:
        man()

    if args.clean:
        clean()

    if args.ctags:
        ctags()

    if args.list:
        print("Supported projects:")
        for supported_project in SUPPORTED_PROJECTS:
            print(f"-- {supported_project}")

    if args.menuconfig:
        enable_menuconfig()

    project = args.build
    toolchain_path = args.toolchain

    if project is not None:
        if project not in SUPPORTED_PROJECTS:
            raise Exception(f"{project} is not supported, check supported projects by '--list'.")
        else:
            if toolchain_path is None:
                raise Exception("'-t,--toolchain' must set.")

            build_zephyr(project, toolchain_path)

if __name__ == "__main__":
    main()
