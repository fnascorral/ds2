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
#include "DebugServer2/Host/Linux/X86/Syscalls.h"
#include "DebugServer2/Host/Linux/X86_64/Syscalls.h"
#include "DebugServer2/Target/Thread.h"

namespace X86Sys = ds2::Host::Linux::X86::Syscalls;
namespace X86_64Sys = ds2::Host::Linux::X86_64::Syscalls;

namespace ds2 {
namespace Target {
namespace Linux {

static bool is32BitProcess(Process *process) {
  DS2ASSERT(process != nullptr);
  DS2ASSERT(process->currentThread() != nullptr);

  Architecture::CPUState state;
  ErrorCode error = process->currentThread()->readCPUState(state);
  if (error != kSuccess) {
    return error;
  }

  return state.is32;
}

ErrorCode Process::allocateMemory(size_t size, uint32_t protection,
                                  uint64_t *address) {
  if (address == nullptr) {
    return kErrorInvalidArgument;
  }

  bool is32 = is32BitProcess(this);

  ByteVector codestr;
  if (is32) {
    X86Sys::PrepareMmapCode(size, protection, codestr);
  } else {
    X86_64Sys::PrepareMmapCode(size, protection, codestr);
  }

  uint64_t result;
  ErrorCode error = executeCode(codestr, result);
  if (error != kSuccess) {
    return error;
  }

  // MAP_FAILED is -1.
  if ((is32 && static_cast<int64_t>(result) == -1LL) ||
      (!is32 && static_cast<int32_t>(result) == -1)) {
    return kErrorNoMemory;
  }

  *address = result;
  return kSuccess;
}

ErrorCode Process::deallocateMemory(uint64_t address, size_t size) {
  if (size == 0) {
    return kErrorInvalidArgument;
  }

  ByteVector codestr;
  if (is32BitProcess(this)) {
    X86Sys::PrepareMunmapCode(address, size, codestr);
  } else {
    X86_64Sys::PrepareMunmapCode(address, size, codestr);
  }

  uint64_t result;
  ErrorCode error = executeCode(codestr, result);
  if (error != kSuccess) {
    return error;
  }

  // Negative values returned by the kernel indicate failure.
  if (static_cast<int32_t>(result) < 0) {
    return kErrorInvalidArgument;
  }

  return kSuccess;
}
}
}
}
