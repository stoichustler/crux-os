# SPDX-License-Identifier: Apache-2.0

add_library(CRUX_CMD INTERFACE)

zephyr_library()
zephyr_library_sources(src/crux_cmd.c)
zephyr_library_link_libraries(CRUX_CMD)
