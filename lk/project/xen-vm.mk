# main project for qemu-aarch64
MODULES += \
	app/shell \
	# app/lkboot \
	# app/loader

#######################################################
#                      _____     __  __
#             /\/\/\/\/ __  \/\ / _\/__\
#             \ - \ \ \ \/ // // _\/  \
#              \/\/\_/\_/\/ \__\__\\/\/ @2025
#
#                  - Hustle Embedded -
#
# Todo: lkboot/loader
#######################################################

include project/virtual/test.mk
include project/virtual/fs.mk
include project/virtual/minip.mk
include project/target/xen-vm-arm64.mk
