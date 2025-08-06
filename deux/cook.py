#############################################
#         _______  __  __
#        /__\_  _\/  \/ _\
#       /  \ / / / / /\ \
#       \/\/ \/  \__/\__/ DEUX 2025
#
#############################################

from argparse import ArgumentParser
from os import system, environ
from pathlib import Path

import time

BUILD_DIR = "build"

def print_banner() -> None:
    print(
"  __  __      __\n" \
" / _\\/__\\/\\/\\/ _\\\n" \
"/ / /  \\/ / /\\ \\\n" \
"\\_\\ \\/\\/\\__/\\__/ CRUX 2025\n" \
    )

def countdown(secs) -> None:
    for i in range(secs, 0, -1):
        print(f'\r-- Start building in {i} secs', end="", flush=True)
        time.sleep(1)
    print("\n\n")

def ctags() -> None:
    print("-- Generating tags ...\n")
    ret = system('ctags --languages=Asm,c,c++ -R')
    assert ret == 0

def clean() -> None:
    print("-- Cleaning built objects ...\n")
    ret = system(f'rm -rf {BUILD_DIR}')
    assert ret == 0

def build(
    toolchain_file: str,
) -> None:
    """
        build zeus
    """
    if toolchain_file is None:
        print("-- target toolchain file is needed. check cmake/")
        return

    ret = system(f'rm -rf {BUILD_DIR}')
    assert ret == 0

    pre_cmd = f'cmake -B{BUILD_DIR} -GNinja -DCMAKE_TOOLCHAIN_FILE={toolchain_file}'
    ret = system(pre_cmd)
    assert ret == 0

    # Wait 3 seconds ...
    countdown(3)

    ninja_valid = Path(f'{BUILD_DIR}/build.ninja')
    if ninja_valid.exists():
        fin_cmd = f'cmake --build {BUILD_DIR}'
        ret = system(fin_cmd)
        assert ret == 0


def main() -> None:
    parser = ArgumentParser()
    parser.add_argument("-b", "--build", action="store_true", help="Build deux target")
    parser.add_argument("-t", "--toolchain", metavar="TOOLCHAIN-FILE", help="Target toolchain file")
    parser.add_argument("-g", "--ctags", action="store_true", help="Create tags")
    parser.add_argument("-c", "--clean", action="store_true", help="Clean built target")

    args = parser.parse_args()

    # personal interest, never mind.
    print_banner()

    if args.clean:
        clean()

    if args.ctags:
        ctags()

    toolchain_file = args.toolchain

    if args.build:
        build(toolchain_file)

if __name__ == "__main__":
    main()
