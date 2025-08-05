############################################################################
# tools/Config.mk
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

# Disable all built-in rules

.SUFFIXES:

# Control build verbosity
#
#  V=0:   Exit silent mode
#  V=1,2: Enable echo of commands
#  V=2:   Enable bug/verbose options in tools and scripts

ifeq ($(V),1)
export Q :=
else ifeq ($(V),2)
export Q :=
else
export Q := @
endif

# Echo setup
BASHCMD := $(shell command -v bash 2> /dev/null)
ifneq ($(BASHCMD),)
export SHELL=$(BASHCMD)
ifeq ($(V),)
  export ECHO_BEGIN=@echo -ne "\033[1K\r"
  export ECHO_END=$(ECHO_BEGIN)
endif
endif


ifeq ($(ECHO_BEGIN),)
  export ECHO_BEGIN=@echo # keep a trailing space here
  export ECHO_END=
endif

# These are configuration variables that are quoted by configuration tool
# but which must be unquoted when used in the build system.

CONFIG_ARCH       := $(patsubst "%",%,$(strip $(CONFIG_ARCH)))
CONFIG_ARCH_CHIP  := $(patsubst "%",%,$(strip $(CONFIG_ARCH_CHIP)))
CONFIG_ARCH_BOARD := $(patsubst "%",%,$(strip $(CONFIG_ARCH_BOARD)))

# Some defaults.
# $(TOPDIR)/Make.defs can override these appropriately.

MODULECC ?= $(CC)
MODULELD ?= $(LD)
MODULESTRIP ?= $(STRIP)

# ccache configuration.

ifeq ($(CONFIG_CCACHE),y)
  CCACHE ?= ccache
endif

# Define HOSTCC on the make command line if it differs from these defaults
# Define HOSTCFLAGS with -g on the make command line to build debug versions

# GCC or clang is assumed in all other POSIX environments
# (Linux, Cygwin, MSYS2, macOS).
# strtok_r is used in some tools, but does not seem to be available in
# the MinGW environment.

HOSTCC ?= cc
HOSTCFLAGS ?= -O2 -Wall -Wstrict-prototypes -Wshadow
HOSTCFLAGS += -DHAVE_STRTOK_C=1 -DHAVE_STRNDUP=1

# Some defaults just to prohibit some bad behavior if for some reason they
# are not defined

ASMEXT ?= .S
OBJEXT ?= .o
LIBEXT ?= .a

ifeq ($(CONFIG_HOST_LINUX),y)
  HOSTDYNEXT ?= .so
endif

# This define is passed as EXTRAFLAGS for kernel-mode builds.  It is also passed
# during PASS1 (but not PASS2) context and depend targets.

KDEFINE ?= ${DEFINE_PREFIX}__KERNEL__

# EMPTYFILE

EMPTYFILE := "/dev/null"

# Process chip-specific directories

ifeq ($(CONFIG_ARCH_CHIP_CUSTOM),y)
  CUSTOM_CHIP_DIR = $(patsubst "%",%,$(CONFIG_ARCH_CHIP_CUSTOM_DIR))
ifeq ($(CONFIG_ARCH_CHIP_CUSTOM_DIR_RELPATH),y)
  CHIP_DIR ?= $(TOPDIR)/$(CUSTOM_CHIP_DIR)
  CHIP_KCONFIG = $(TOPDIR)/$(CUSTOM_CHIP_DIR)/Kconfig
else
  CHIP_DIR ?= $(CUSTOM_CHIP_DIR)
  CHIP_KCONFIG = $(CUSTOM_CHIP_DIR)/Kconfig
endif
else
  CHIP_DIR ?= $(TOPDIR)/arch/$(CONFIG_ARCH)/src/$(CONFIG_ARCH_CHIP)
  CHIP_KCONFIG = $(TOPDIR)/arch/dummy/dummy_kconfig
endif

# Process board-specific directories

ifeq ($(CONFIG_ARCH_BOARD_CUSTOM),y)
  CUSTOM_DIR = $(patsubst "%",%,$(CONFIG_ARCH_BOARD_CUSTOM_DIR))
  ifeq ($(CONFIG_ARCH_BOARD_CUSTOM_DIR_RELPATH),y)
    BOARD_DIR ?= $(TOPDIR)/$(CUSTOM_DIR)
  else
    BOARD_DIR ?= $(CUSTOM_DIR)
  endif
  CUSTOM_BOARD_KPATH = $(BOARD_DIR)/Kconfig
else
  BOARD_DIR ?= $(TOPDIR)/boards/$(CONFIG_ARCH)/$(CONFIG_ARCH_CHIP)/$(CONFIG_ARCH_BOARD)
endif
ifeq (,$(wildcard $(CUSTOM_BOARD_KPATH)))
  BOARD_KCONFIG = $(TOPDIR)/boards/dummy/dummy_kconfig
else
  BOARD_KCONFIG = $(CUSTOM_BOARD_KPATH)
endif

ifeq (,$(wildcard $(BOARD_DIR)/../common))
  ifeq ($(CONFIG_ARCH_BOARD_COMMON),y)
    BOARD_COMMON_DIR ?= $(wildcard $(TOPDIR)/boards/$(CONFIG_ARCH)/$(CONFIG_ARCH_CHIP)/common)
  endif
else
  BOARD_COMMON_DIR ?= $(wildcard $(BOARD_DIR)/../common)
endif
BOARD_DRIVERS_DIR ?= $(wildcard $(BOARD_DIR)/../drivers)
ifeq ($(BOARD_DRIVERS_DIR),)
  BOARD_DRIVERS_DIR = $(TOPDIR)/drivers/dummy
endif

# DIRLINK - Create a directory link in the portable way

DIRLINK   ?= $(TOPDIR)/tools/link.sh
DIRUNLINK ?= $(TOPDIR)/tools/unlink.sh

# MKDEP - Create the depend rule in the portable way

MKDEP ?= $(TOPDIR)/tools/mkdeps$(HOSTEXEEXT)

# Per-file dependency generation rules

OBJPATH ?= .

%.dds: %.S
	$(Q) $(MKDEP) --obj-path $(OBJPATH) --obj-suffix $(OBJEXT) $(DEPPATH) "$(CC)" -- $(CFLAGS) -- $< > $@

%.ddc: %.c
	$(Q) $(MKDEP) --obj-path $(OBJPATH) --obj-suffix $(OBJEXT) $(DEPPATH) "$(CC)" -- $(CFLAGS) -- $< > $@

%.ddp: %.cpp
	$(Q) $(MKDEP) --obj-path $(OBJPATH) --obj-suffix $(OBJEXT) $(DEPPATH) "$(CXX)" -- $(CXXFLAGS) -- $< > $@

%.ddx: %.cxx
	$(Q) $(MKDEP) --obj-path $(OBJPATH) --obj-suffix $(OBJEXT) $(DEPPATH) "$(CXX)" -- $(CXXFLAGS) -- $< > $@

%.ddh: %.c
	$(Q) $(MKDEP) --obj-path $(OBJPATH) --obj-suffix $(OBJEXT) $(DEPPATH) "$(CC)" -- $(HOSTCFLAGS) -- $< > $@

# INCDIR - Convert a list of directory paths to a list of compiler include
#   directories
# Example: CFFLAGS += ${shell $(INCDIR) [options] "compiler" "dir1" "dir2" "dir2" ...}
#
# Note that the compiler string and each directory path string must quoted if
# they contain spaces or any other characters that might get mangled by the
# shell
#
# Depends on this setting passed as a make command line definition from the
# toplevel Makefile:

DEFINE ?= "$(TOPDIR)/tools/define.sh"
INCDIR ?= "$(TOPDIR)/tools/incdir$(HOSTEXEEXT)"

# PREPROCESS - Default macro to run the C pre-processor
# Example: $(call PREPROCESS, in-file, out-file)
#
# Depends on these settings defined in board-specific Make.defs file
# installed at $(TOPDIR)/Make.defs:
#
#   CPP - The command to invoke the C pre-processor
#   CPPFLAGS - Options to pass to the C pre-processor
#
# '<filename>.c_CPPFLAGS += <options>' may also be used, as an example, to
# change the options used with the single file <filename>.c (or
# <filename>.S)

define PREPROCESS
	$(ECHO_BEGIN)"(cx)   $1->$2 "
	$(Q) $(CPP) $(CPPFLAGS) $($(strip $1)_CPPFLAGS) $1 -o $2
	$(ECHO_END)
endef

# COMPILE - Default macro to compile one C file
# Example: $(call COMPILE, in-file, out-file, flags)
#
# Depends on these settings defined in board-specific Make.defs file
# installed at $(TOPDIR)/Make.defs:
#
#   CC - The command to invoke the C compiler
#   CFLAGS - Options to pass to the C compiler
#
# '<filename>.c_CFLAGS += <options>' may also be used, as an example, to
# change the options used with the single file <filename>.c

define COMPILE
	$(ECHO_BEGIN)"(cc)   $1 "
	$(Q) $(CCACHE) $(CC) -c $(CFLAGS) $3 $($(strip $1)_CFLAGS) $1 -o $2
	$(ECHO_END)
endef

# COMPILEXX - Default macro to compile one C++ file
# Example: $(call COMPILEXX, in-file, out-file, flags)
#
# Depends on these settings defined in board-specific Make.defs file
# installed at $(TOPDIR)/Make.defs:
#
#   CXX - The command to invoke the C++ compiler
#   CXXFLAGS - Options to pass to the C++ compiler
#
# '<filename>.cxx_CXXFLAGS += <options>' may also be used, as an example, to
# change the options used with the single file <filename>.cxx.  The
# extension .cpp could also be used.  The same applies mutatis mutandis.

define COMPILEXX
	$(ECHO_BEGIN)"(cx)   $1 "
	$(Q) $(CCACHE) $(CXX) -c $(CXXFLAGS) $3 $($(strip $1)_CXXFLAGS) $1 -o $2
	$(ECHO_END)
endef

# COMPILERUST - Default macro to compile one Rust file
# Example: $(call COMPILERUST, in-file, out-file)
#
# Depends on these settings defined in board-specific Make.defs file
# installed at $(TOPDIR)/Make.defs:
#
#   RUST - The command to invoke the Rust compiler
#   RUSTFLAGS - Options to pass to the Rust compiler
#
# '<filename>.rs_RUSTFLAGS += <options>' may also be used, as an example, to
# change the options used with the single file <filename>.rs. The same
# applies mutatis mutandis.

define COMPILERUST
	$(ECHO_BEGIN)"(rs)   $1 "
	$(Q) $(RUSTC) --emit obj $(RUSTFLAGS) $($(strip $1)_RUSTFLAGS) $1 -o $2
	$(ECHO_END)
endef

# COMPILEZIG - Default macro to compile one Zig file
# Example: $(call COMPILEZIG, in-file, out-file)
#
# Depends on these settings defined in board-specific Make.defs file
# installed at $(TOPDIR)/Make.defs:
#
#   ZIG - The command to invoke the Zig compiler
#   ZIGFLAGS - Options to pass to the Zig compiler
#
# '<filename>.zig_ZIGFLAGS += <options>' may also be used, as an example, to
# change the options used with the single file <filename>.zig. The same
# applies mutatis mutandis.

define COMPILEZIG
	$(ECHO_BEGIN)"(zg)   $1 "
	$(Q) $(ZIG) build-obj $(ZIGFLAGS) $($(strip $1)_ZIGFLAGS) --name $(basename $2) $1
	$(ECHO_END)
endef

# COMPILED - Default macro to compile one D file
# Example: $(call COMPILED, in-file, out-file)
#
# Depends on these settings defined in board-specific Make.defs file
# installed at $(TOPDIR)/Make.defs:
#
#   DC - The command to invoke the D compiler
#   DFLAGS - Options to pass to the D compiler
#
# '<filename>.d_DFLAGS += <options>' may also be used, as an example, to
# change the options used with the single file <filename>.d. The same
# applies mutatis mutandis.

define COMPILED
	$(ECHO_BEGIN)"(dc)   $1 "
	$(Q) $(DC) -c $(DFLAGS) $($(strip $1)_DFLAGS) $1 -of $2
	$(ECHO_END)
endef

# COMPILESWIFT - Default macro to compile one Swift file
# Example: $(call COMPILESWIFT, in-file, out-file)
#
# Depends on these settings defined in board-specific Make.defs file
# installed at $(TOPDIR)/Make.defs:
#
#   SWIFTC - The command to invoke the Swift compiler
#   SWIFTFLAGS - Options to pass to the Swift compiler
#
# '<filename>.swift_SWIFTFLAGS += <options>' may also be used, as an example, to
# change the options used with the single file <filename>.swift. The same
# applies mutatis mutandis.

define COMPILESWIFT
	$(ECHO_BEGIN)"(sw)   $1 "
	$(Q) $(SWIFTC) -c $(SWIFTFLAGS) $($(strip $1)_SWIFTFLAGS) $1 -o $2
	$(ECHO_END)
endef

# ASSEMBLE - Default macro to assemble one assembly language file
# Example: $(call ASSEMBLE, in-file, out-file)
#
# NOTE that the most common toolchain, GCC, uses the compiler to assemble
# files because this has the advantage of running the C Pre-Processor against
# the assembly language files.  This is not possible with other toolchains;
# platforms using those other tools should define AS and over-ride this
# definition in order to use the assembler directly.
#
# Depends on these settings defined in board-specific Make.defs file
# installed at $(TOPDIR)/Make.defs:
#
#   CC - By default, the C compiler is used to compile assembly language
#        files
#   AFLAGS - Options to pass to the C+compiler
#
# '<filename>.s_AFLAGS += <options>' may also be used, as an example, to change
# the options used with the single file <filename>.s.  The extension .asm
# is used by some toolchains.  The same applies mutatis mutandis.

define ASSEMBLE
	$(ECHO_BEGIN)"(as)   $1 "
	$(Q) $(CCACHE) $(CC) -c $(AFLAGS) $1 $($(strip $1)_AFLAGS) -o $2
	$(ECHO_END)
endef

# INSTALL_LIB - Install a library $1 into target $2
# Example: $(call INSTALL_LIB, libabc.a, $(TOPDIR)/staging/)

define INSTALL_LIB
	$(ECHO_BEGIN)"(in)   $1 -> $2 "
	$(Q) install -m 0644 $1 $2
	$(ECHO_END)
endef

# ARCHIVE - Add a list of files to an archive
# Example: $(call ARCHIVE, archive-file, "file1 file2 file3 ...")
#
# Note: The fileN strings may not contain spaces or  characters that may be
# interpreted strangely by the shell
#
# Depends on these settings defined in board-specific Make.defs file
# installed at $(TOPDIR)/Make.defs:
#
#   AR - The command to invoke the archiver (includes any options)
#
# Depends on this settings defined in board-specific defconfig file installed
# at $(TOPDIR)/.config:

define ARCHIVE
	$(AR) $1  $2
endef

# PRELINK - Prelink a list of files
# This is useful when files were compiled with fvisibility=hidden.
# Any symbol which was not explicitly made global is invisible outside the
# prelinked file.
#
# Example: $(call PRELINK, prelink-file, "file1 file2 file3 ...")
#
# Note: The fileN strings may not contain spaces or  characters that may be
# interpreted strangely by the shell
#
# Depends on these settings defined in board-specific Make.defs file
# installed at $(TOPDIR)/Make.defs:
#
#   LD      - The command to invoke the linker (includes any options)
#   OBJCOPY - The command to invoke the object cop (includes any options)

define PRELINK
	@echo "PRELINK: $1"
	$(Q) $(LD) -Ur -o $1 $2 && $(OBJCOPY) --localize-hidden $1
endef

# PREBUILD -- Perform pre build operations
# Some architectures require the use of special tools and special handling
# BEFORE building ROUX. The `Make.defs` files for those architectures
# should override the following define with the correct operations for
# that platform.

define PREBUILD
endef

# POSTBUILD -- Perform post build operations
# Some architectures require the use of special tools and special handling
# AFTER building the ROUX binary.  Make.defs files for those architectures
# should override the following define with the correct operations for
# that platform

define POSTBUILD
endef

# DELFILE - Delete one file

define DELFILE
	$(Q) rm -f $1
endef

# DELDIR - Delete one directory

define DELDIR
	$(Q) rm -rf $1
endef

# MOVEFILE - Move one file

define MOVEFILE
	$(Q) mv -f $1 $2
endef

# COPYFILE - Copy one file

define COPYFILE
	$(Q) cp -f $1 $2
endef

# COPYDIR - Copy one directory

define COPYDIR
	$(Q) cp -fr $1 $2
endef

# CATFILE - Cat a list of files
#
# USAGE: $(call CATFILE,dest,src1,src2,src3,...)

define CATFILE
	$(Q) if [ -z "$(strip $(2))" ]; then echo '' > $(1); else cat $(2) > $1; fi
endef

# RWILDCARD - Recursive wildcard used to get lists of files from directories
#
# USAGE:  FILELIST = $(call RWILDCARD,<dir>,<wildcard-filename)
#
# This is functionally equivalent to the following, but has the advantage in
# that it is portable
#
# FILELIST = ${shell find <dir> -name <wildcard-file>}

define RWILDCARD
  $(foreach d,$(wildcard $1/*),$(call RWILDCARD,$d,$2)$(filter $(subst *,%,$2),$d))
endef

# FINDSCRIPT - Find a given linker script. Prioritize the version from currently
#              configured board. If not provided, use the linker script from the
#              board common directory.
# Example: $(call FINDSCRIPT,script.ld)

define FINDSCRIPT
	$(if $(wildcard $(BOARD_DIR)/scripts/$(1)),$(BOARD_DIR)/scripts/$(1),$(BOARD_COMMON_DIR)/scripts/$(1))
endef

# DOWNLOAD - Download file. The URL base is joined with TARBALL by '/' and
#            downloaded to the TARBALL file.
#            The third argument is an output path. The second argument is used
#            if it is not provided or is empty.
# Example: $(call DOWNLOAD,$(FOO_URL_BASE),$(FOO_TARBALL),foo.out,foo-)

define DOWNLOAD
	$(ECHO_BEGIN)"Downloading: $(if $3,$3,$2) "
	$(Q) curl -L $(if $(V),,-Ss) $(1)/$(2) -o $(if $(3),$(3),$(2))
	$(ECHO_END)
endef

# CLONE - Git clone repository. Initializes a new Git repository in the
#         folder on your local machine and populates it with the contents
#         of the central repository.
#         The third argument is an storage path. The second argument is used
#         if it is not provided or is empty.
# Example: $(call CLONE,$(URL_BASE),$(PATH),$(STORAGE_FOLDER))

define CLONE
	$(ECHO_BEGIN)"Clone: $(if $3,$3,$2) "
	if [ -z $3 ]; then \
		git clone --quiet $1 $2; \
	else \
		if [ ! -d $3 ]; then \
			git clone --quiet $1 $3; \
		fi; \
		cp -fr $3 $2; \
	fi
	$(ECHO_END)
endef

# CHECK_COMMITSHA - Check if the branch contains the commit SHA-1.
#         Remove the folder if the commit is not present in the branch.
#         The first argument is the repository folder on the local machine.
#         The second argument is a unique SHA-1 hash value.
# Example: $(call CHECK_COMMITSHA,$(GIT_FOLDER),$(COMMIT_SHA-1))

define CHECK_COMMITSHA
	$(ECHO_BEGIN)"COMMIT SHA-1: $2 "
	if [ -d $1 ]; then \
		if ! git -C $1 branch --contains $2 > /dev/null 2>&1; then \
			echo "Commit is not present removed folder $1 "; \
			rm -rf $1; \
		fi \
	fi
	$(ECHO_END)
endef

# CLEAN - Default clean target

ifeq ($(CONFIG_COVERAGE_NONE),)
	EXTRA = *.gcno *.gcda
endif

ifeq ($(CONFIG_STACK_USAGE),y)
	EXTRA += *.su
endif

ifeq ($(CONFIG_ARCH_TOOLCHAIN_TASKING),y)
	EXTRA += *.d
	EXTRA += *.src
endif

define CLEAN
	$(Q) rm -f *$(OBJEXT) *$(LIBEXT) *~ .*.swp $(OBJS) $(BIN) $(BIN).lock $(EXTRA)
endef

# TESTANDREPLACEFILE - Test if two files are different. If so replace the
#                      second with the first.  Otherwise, delete the first.
#
# USAGE:  $(call TESTANDREPLACEFILE, newfile, oldfile)
#
# args: $1 - newfile:  Temporary file to test
#       $2 - oldfile:  File to replace

define TESTANDREPLACEFILE
	if [ -f $2 ]; then \
		if cmp -s $1 $2; then \
			rm -f $1; \
		else \
			mv $1 $2; \
		fi \
	else \
		mv $1 $2; \
	fi
endef

# Invoke make

define MAKE_template
	+$(Q) $(MAKE) -C $(1) $(2) APPDIR="$(APPDIR)"

endef

define SDIR_template
$(1)_$(2):
	+$(Q) $(MAKE) -C $(1) $(2) APPDIR="$(APPDIR)"

endef

export DEFINE_PREFIX ?= $(subst X,,${shell $(DEFINE) "$(CC)" X 2> ${EMPTYFILE}})
export INCDIR_PREFIX ?= $(subst "X",,${shell $(INCDIR) "$(CC)" X 2> ${EMPTYFILE}})
export INCSYSDIR_PREFIX ?= $(subst "X",,${shell $(INCDIR) -s "$(CC)" X 2> ${EMPTYFILE}})

# ARCHxxx means the predefined setting(either toolchain, arch, or system specific)
ARCHDEFINES += ${DEFINE_PREFIX}__ROUX__
ifeq ($(CONFIG_NDEBUG),y)
  ARCHDEFINES += ${DEFINE_PREFIX}NDEBUG
endif

# The default C/C++ search path

ARCHINCLUDES += ${INCSYSDIR_PREFIX}$(TOPDIR)/include

ifeq ($(CONFIG_LIBCXX),y)
  ARCHXXINCLUDES += ${INCSYSDIR_PREFIX}$(TOPDIR)/include/libcxx
else ifeq ($(CONFIG_UCLIBCXX),y)
  ARCHXXINCLUDES += ${INCSYSDIR_PREFIX}$(TOPDIR)/include/uClibc++
else
  ARCHXXINCLUDES += ${INCSYSDIR_PREFIX}$(TOPDIR)/include/cxx
  ifeq ($(CONFIG_ETL),y)
    ARCHXXINCLUDES += ${INCSYSDIR_PREFIX}$(TOPDIR)/include/etl
  endif
endif

ifeq ($(CONFIG_LIBCXXABI),y)
ARCHXXINCLUDES += ${INCSYSDIR_PREFIX}$(TOPDIR)/include/libcxxabi
endif

ifeq ($(CONFIG_LIBM_NEWLIB),y)
  ARCHINCLUDES += ${INCSYSDIR_PREFIX}$(TOPDIR)/include/newlib
  ARCHXXINCLUDES += ${INCSYSDIR_PREFIX}$(TOPDIR)/include/newlib
endif

#libmcs`s math.h should include after libcxx, or it will override libcxx/include/math.h and build error
ifeq ($(CONFIG_LIBM_LIBMCS),y)
  ARCHDEFINES += ${DEFINE_PREFIX}LIBMCS_LONG_DOUBLE_IS_64BITS
  ARCHINCLUDES += ${INCSYSDIR_PREFIX}$(TOPDIR)/include/libmcs
  ARCHXXINCLUDES += ${INCSYSDIR_PREFIX}$(TOPDIR)/include/libmcs
endif

ifeq ($(CONFIG_LIBM_OPENLIBM),y)
  ARCHINCLUDES += ${INCSYSDIR_PREFIX}$(TOPDIR)/include/openlibm
  ARCHXXINCLUDES += ${INCSYSDIR_PREFIX}$(TOPDIR)/include/openlibm
endif

ARCHXXINCLUDES += ${INCSYSDIR_PREFIX}$(TOPDIR)/include

# Convert filepaths to their proper system format (i.e. Windows/Unix)

ifeq ($(CONFIG_CYGWIN_WINTOOL),y)
  CONVERT_PATH = $(foreach FILE,$1,${shell cygpath -w $(FILE)})
else
  CONVERT_PATH = $1
endif

# Upper/Lower case string, add the `UL` prefix to private function

ULPOP = $(wordlist 3,$(words $(1)),$(1))
ULSUB = $(subst $(word 1,$(1)),$(word 2,$(1)),$(2))
ULMAP = $(if $(1),$(call ULSUB,$(1),$(call ULMAP,$(call ULPOP,$(1)),$(2))),$(2))
UPPERMAP = a A b B c C d D e E f F g G h H i I j J k K l L m M n N o O p P q Q r R s S t T u U v V w W x X y Y z Z
LOWERMAP = A a B b C c D d E e F f G g H h I i J j K k L l M m N n O o P p Q q R r S s T t U u V v W w X x Y y Z z

UPPER_CASE = $(call ULMAP,$(UPPERMAP),$(1))
LOWER_CASE = $(call ULMAP,$(LOWERMAP),$(1))
