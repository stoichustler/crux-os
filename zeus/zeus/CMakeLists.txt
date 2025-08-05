# SPDX-License-Identifier: Apache-2.0

# python libs requirements:
# [1] pip install packaging pyelftools pykwalify pyyaml

cmake_minimum_required(VERSION 3.20.0)

#######################################################
# NOTE: enssential setup for building zephyr with CMake
# only:

set(ZEPHYR_TOOLCHAIN_VARIANT cross-compile)

set(BUILD_VERSION 1.0.0)
#######################################################
# NOTE: target board

if (BOARD STREQUAL "radxa_zero3w/rk3568/xen_dom0")
  if(NOT xen_dom0 IN_LIST SNIPPET)
    set(SNIPPET xen_dom0 ${SNIPPET} CACHE STRING "" FORCE)
  endif()
endif()

if (BOARD STREQUAL "qemu_cortex_a53/qemu_cortex_a53")
  if(NOT xen_dom0 IN_LIST SNIPPET)
    set(SNIPPET xen_dom0 ${SNIPPET} CACHE STRING "" FORCE)
  endif()
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(zeus)

# User application
target_sources(app PRIVATE src/zeus.c)

zephyr_sources_ifdef(CONFIG_XEN_DOM0 src/xcmd.c)

# Latency measure sources
FILE(GLOB latency_sources src/latency/*.c)
zephyr_sources_ifdef(CONFIG_TEST ${latency_sources})

zephyr_sources(
	src/coremark/core_main.c
	src/coremark/core_matrix.c
	src/coremark/core_state.c
	src/coremark/core_util.c
	src/coremark/core_list_join.c
	src/coremark/core_portme.c
	src/coremark/core_shcmd.c
)


#######################################################
