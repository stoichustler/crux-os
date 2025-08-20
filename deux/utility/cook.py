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

def clean() -> None:
    ret = system(f'rm -rf {OUTPUT}')
    assert ret == 0

def build() -> None:
    toolchain_file = "cmake/aarch64.cmake"
    ret = system(f'cmake -B{OUTPUT} -GNinja -DCMAKE_TOOLCHAIN_FILE={toolchain_file}')
    assert ret == 0
	# Compilation
    ret = system(f'cmake --build {OUTPUT}')
    assert ret == 0

def main() -> None:
    parser = ArgumentParser()
    parser.add_argument("-b", "--build", action="store_true", help="build u-boot")
    parser.add_argument("-c", "--clean", action="store_true", help="clean u-boot")

    args = parser.parse_args()

    if args.build:
        build()

    if args.clean:
        clean()

if __name__ == "__main__":
    main()
