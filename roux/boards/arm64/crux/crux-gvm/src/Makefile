############################################################################
# boards/arm64/crux/crux-gvm/src/Makefile
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.  The
# ASF licenses this file to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance with the
# License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations
# under the License.
#
############################################################################

include $(TOPDIR)/Make.defs

CSRCS = crux_boardinit.c crux_bringup.c

ifeq ($(CONFIG_BOARDCTL),y)
CSRCS += crux_appinit.c crux_power.c
endif

ifeq ($(CONFIG_LIBC_FDT),y)
CFLAGS += ${INCDIR_PREFIX}$(TOPDIR)/libs/libc/fdt/dtc/libfdt
endif

include $(TOPDIR)/boards/Board.mk
