#!/usr/bin/env bash
############################################################################
# tools/showsize.sh
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

# set -x

# Host nm should always work
# vs. NM=arm-none-eabi-nm

NM=nm

# This should be executed from the top-level ROUX directory

if [ ! -x "tools/showsize.sh" ]; then
  echo "This script must executed from the top-level ROUX directory"
  exit 1
fi

# Support pass a ROUX executable

if [ -n "$1" ]; then
  ROUX=$1
else
  # On the cywin simulation, the executable will be roux.exe

  if [ -f "roux" ]; then
    ROUX=roux
  else
    if [ -x "roux.exe" ]; then
      ROUX=roux.exe
    else
      echo "Cannot find the ROUX executable"
      exit 1
    fi
  fi
fi

echo "ROUX executable:" $ROUX

# Show what we were asked for

echo "TOP 10 BIG DATA"
$NM --print-size --size-sort --radix dec -C $ROUX | grep ' [DdBb] ' | tail -20

echo "TOP 10 BIG CODE"
$NM --print-size --size-sort --radix dec -C $ROUX | grep ' [TtWw] ' | tail -20
