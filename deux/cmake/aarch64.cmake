#######################################################
#                      _____     __  __
#             /\/\/\/\/ __  \/\ / _\/__\
#             \ - \ \ \ \/ // // _\/  \
#              \/\/\_/\_/\/ \__\__\\/\/ @2025
#
#                  - Hustle Embedded -
#######################################################

set(CMAKE_SYSTEM_NAME Generic)

set(THREADX_ARCH "aarch64")
set(THREADX_TOOLCHAIN "gnu")

set(MCPU_FLAGS "-mcpu=cortex-a55")
set(VFP_FLAGS "")

include(${CMAKE_CURRENT_LIST_DIR}/aarch64-none-elf.cmake)
