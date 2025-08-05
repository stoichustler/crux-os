#!/usr/bin/env bash
# tools/simbridge.sh
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to you under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

if [ $# != 2 ]; then
  echo "Usage: $0 <interface> <on|off>"
  exit 1
fi

if [ "$2" == "on" ]; then
  ip link add roux0 type bridge
  ip addr flush dev $1
  ip link set $1 master roux0
  ip link set dev roux0 up
  dhclient roux0
else
  ip link delete roux0
  dhclient $1
fi
