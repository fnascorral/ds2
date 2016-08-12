//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#include "DebugServer2/Target/Process.h"

#include <cstdlib>
#include <sys/mman.h>
#include <sys/syscall.h>

namespace ds2 {
namespace Target {
namespace Linux {

ErrorCode Process::allocateMemory(size_t size, uint32_t protection,
                                  uint64_t *address) {
  return kErrorUnsupported;
}

ErrorCode Process::deallocateMemory(uint64_t address, size_t size) {
  return kErrorUnsupported;
}
}
}
}
