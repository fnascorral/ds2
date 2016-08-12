#!/usr/bin/env python
##
## Copyright (c) 2014-present, Facebook, Inc.
## All rights reserved.
##
## This source code is licensed under the University of Illinois/NCSA Open
## Source License found in the LICENSE file in the root directory of this
## source tree. An additional grant of patent rights can be found in the
## PATENTS file in the same directory.
##

import os
import sys
from subprocess import check_call

repositories = []
keys = []

target = os.getenv('TARGET')

if 'Darwin' in target:
   check_call('brew update', shell=True)
   sys.exit(0)

if target in [ 'Style', 'Registers', 'Linux-X86', 'Linux-X86_64', 'Tizen-X86' ]:
    repositories.append('ppa:ubuntu-toolchain-r/test')

for r in repositories:
  check_call('sudo add-apt-repository -y "%s"' % r, shell=True)

for k in keys:
  check_call('wget -O - "%s" | sudo apt-key add -' % k, shell=True)

check_call('sudo apt-get update', shell=True)
