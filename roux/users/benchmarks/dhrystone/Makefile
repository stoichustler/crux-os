############################################################################
# apps/benchmarks/dhrystone/Makefile
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

#dhrystone

PROGNAME  = $(CONFIG_BENCHMARK_DHRYSTONE_PROGNAME)
PRIORITY  = $(CONFIG_BENCHMARK_DHRYSTONE_PRIORITY)
STACKSIZE = $(CONFIG_BENCHMARK_DHRYSTONE_STACKSIZE)
MODULE    = $(CONFIG_BENCHMARK_DHRYSTONE)

CFLAGS   += -DMSC_CLOCK -Wno-implicit-int -Wno-strict-prototypes
CFLAGS   += -Wno-implicit-function-declaration -Wno-return-type
CFLAGS   += -Wno-maybe-uninitialized

clean::
	$(call DELFILE, dhrystone/v2.1/*.o)
	$(call DELFILE, dhrystone/v2.1/*.gcno)
	$(call DELFILE, dhrystone/v2.2/*.o)
	$(call DELFILE, dhrystone/v2.2/*.gcno)

CSRCS    += dhrystone/v2.1/dhry_2.c
MAINSRC   = dhrystone/v2.2/dry.c

include $(APPDIR)/Application.mk
