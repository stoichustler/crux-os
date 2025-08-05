############################################################################
# apps/benchmarks/tinymembench/Makefile
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

include $(APPDIR)/Make.defs

############################################################################
# Applications Configuration
############################################################################

PROGNAME    = tinymembench
PRIORITY    = $(CONFIG_BENCHMARK_TINY_MEMBENCH_PRIORITY)
STACKSIZE   = $(CONFIG_BENCHMARK_TINY_MEMBENCH_STACKSIZE)
MODULE      = $(CONFIG_BENCHMARK_TINY_MEMBENCH)

TINYMB_UNPACK   = tinymembench

clean::
	$(call DELFILE, $(TINYMB_UNPACK)/*.o)
	$(call DELFILE, $(TINYMB_UNPACK)/*.gcno)

ASRCS      += $(TINYMB_UNPACK)/aarch64-asm.S
ASRCS      += $(TINYMB_UNPACK)/arm-neon.S
ASRCS      += $(TINYMB_UNPACK)/x86-sse2.S
ASRCS      += $(TINYMB_UNPACK)/mips-32.S

CSRCS      += $(TINYMB_UNPACK)/asm-opt.c
CSRCS      += $(TINYMB_UNPACK)/util.c

MAINSRC     = $(TINYMB_UNPACK)/main.c

CFLAGS     += -Wno-unused-but-set-variable -Wno-unused-variable
CFLAGS     += -Wno-strict-prototypes

include $(APPDIR)/Application.mk
