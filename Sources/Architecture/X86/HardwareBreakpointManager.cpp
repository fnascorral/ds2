//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#include "DebugServer2/Architecture/X86/HardwareBreakpointManager.h"
#include "DebugServer2/Target/Process.h"
#include "DebugServer2/Target/Thread.h"
#include "DebugServer2/Utils/Bits.h"
#include "DebugServer2/Utils/Log.h"

#include <algorithm>

#define super ds2::BreakpointManager

using ds2::Utils::EnableBit;
using ds2::Utils::DisableBit;

namespace ds2 {
namespace Architecture {
namespace X86 {

static const int kMaxHWStoppoints = 4; // dr0, dr1, dr2, dr3
static const int kStatusRegIdx = 6;
static const int kCtrlRegIdx = 7;

HardwareBreakpointManager::HardwareBreakpointManager(
    Target::ProcessBase *process)
    : super(process), _locations(kMaxHWStoppoints, 0) {}

HardwareBreakpointManager::~HardwareBreakpointManager() {}

ErrorCode HardwareBreakpointManager::add(Address const &address, Type type,
                                         size_t size, Mode mode) {
  DS2ASSERT(_sites.size() <= kMaxHWStoppoints);

  if (mode == kModeRead) {
    DS2LOG(Warning,
           "read-only watchpoints are unsupported, setting as read-write");
    mode = static_cast<Mode>(mode | kModeWrite);
  }

  return super::add(address, type, size, mode);
}

ErrorCode HardwareBreakpointManager::remove(Address const &address) {
  auto loc = std::find(_locations.begin(), _locations.end(), address);
  if (loc != _locations.end()) {
    _locations[loc - _locations.begin()] = 0;
  }

  return super::remove(address);
}

int HardwareBreakpointManager::maxWatchpoints() { return kMaxHWStoppoints; }

ErrorCode HardwareBreakpointManager::enableLocation(Site const &site, int idx,
                                                    Target::Thread *thread) {
  ErrorCode error;

  error = thread->writeDebugReg(idx, site.address);
  if (error != kSuccess) {
    DS2LOG(Error, "failed to write to debug address register");
    return error;
  }

  uint32_t ctrlReg = thread->readDebugReg(kCtrlRegIdx);

  error = enableDebugCtrlReg(ctrlReg, idx, site.mode, site.size);
  if (error != kSuccess) {
    return error;
  }

  error = thread->writeDebugReg(kCtrlRegIdx, ctrlReg);
  if (error != kSuccess) {
    DS2LOG(Error, "failed to write to debug control register");
    return error;
  }

  error = thread->writeDebugReg(kStatusRegIdx, 0);
  if (error != kSuccess) {
    DS2LOG(Error, "failed to clear debug status register");
    return error;
  }

  return kSuccess;
}

ErrorCode HardwareBreakpointManager::enableLocation(Site const &site) {
  int idx;

  auto loc = std::find(_locations.begin(), _locations.end(), site.address);
  if (loc == _locations.end()) {
    idx = getAvailableLocation();
    if (idx < 0) {
      return kErrorInvalidArgument;
    }
  } else {
    idx = loc - _locations.begin();
  }

  std::set<Target::Thread *> threads;
  _process->enumerateThreads([&](Target::Thread *thread) {
    if (thread->state() == Target::Thread::kStopped) {
      threads.insert(thread);
    }
  });

  for (auto thread : threads) {
    ErrorCode error = enableLocation(site, idx, thread);
    if (error != kSuccess) {
      return error;
    }
  }

  _locations[idx] = site.address;
  return kSuccess;
}

ErrorCode HardwareBreakpointManager::disableLocation(int idx,
                                                     Target::Thread *thread) {
  ErrorCode error;

  error = thread->writeDebugReg(idx, 0);
  if (error != kSuccess) {
    DS2LOG(Error, "failed to clear debug address register: dr%d", idx);
    return error;
  }

  error = thread->writeDebugReg(idx, 0);
  if (error != kSuccess) {
    DS2LOG(Error, "failed to clear addresss register");
    return error;
  }

  uint32_t ctrlReg = thread->readDebugReg(kCtrlRegIdx);

  error = disableDebugCtrlReg(ctrlReg, idx);
  if (error != kSuccess) {
    return error;
  }

  error = thread->writeDebugReg(kCtrlRegIdx, ctrlReg);
  if (error != kSuccess) {
    DS2LOG(Error, "failed to write to debug control register");
    return error;
  }

  return kSuccess;
}

ErrorCode HardwareBreakpointManager::disableLocation(Site const &site) {
  auto loc = std::find(_locations.begin(), _locations.end(), site.address);
  if (loc == _locations.end()) {
    return kErrorInvalidArgument;
  }
  int idx = loc - _locations.begin();

  std::set<Target::Thread *> threads;
  _process->enumerateThreads([&](Target::Thread *thread) {
    if (thread->state() == Target::Thread::kStopped) {
      threads.insert(thread);
    }
  });

  for (auto thread : threads) {
    DS2ASSERT(static_cast<uintptr_t>(site.address) ==
              static_cast<uintptr_t>(thread->readDebugReg(idx)));
    ErrorCode error = disableLocation(idx, thread);
    if (error != kSuccess) {
      return error;
    }
  }

  return kSuccess;
}

ErrorCode HardwareBreakpointManager::enableDebugCtrlReg(uint32_t &ctrlReg,
                                                        int idx, Mode mode,
                                                        int size) {
  int enableIdx = 1 + (idx * 2);
  int infoIdx = 16 + (idx * 4);

  // Set G<idx> flag
  EnableBit(ctrlReg, enableIdx);

  // Set R/W<idx> flags
  switch (static_cast<int>(mode)) {
  case kModeExec:
    DisableBit(ctrlReg, infoIdx);
    DisableBit(ctrlReg, infoIdx + 1);
    break;
  case kModeWrite:
    EnableBit(ctrlReg, infoIdx);
    DisableBit(ctrlReg, infoIdx + 1);
    break;
  case kModeRead:
  case kModeRead | kModeWrite:
    EnableBit(ctrlReg, infoIdx);
    EnableBit(ctrlReg, infoIdx + 1);
    break;
  default:
    return kErrorInvalidArgument;
  }

  // Set LEN<idx> flags, always 0 for exec breakpoints
  if (mode == kModeExec) {
    DisableBit(ctrlReg, infoIdx + 2);
    DisableBit(ctrlReg, infoIdx + 3);
  } else {
    switch (size) {
    case 1:
      DisableBit(ctrlReg, infoIdx + 2);
      DisableBit(ctrlReg, infoIdx + 3);
      break;
    case 2:
      EnableBit(ctrlReg, infoIdx + 2);
      DisableBit(ctrlReg, infoIdx + 3);
      break;
    case 4:
      EnableBit(ctrlReg, infoIdx + 2);
      EnableBit(ctrlReg, infoIdx + 3);
      break;
    case 8:
      DS2LOG(Warning, "8-byte hw breakpoints not supported on all devices");
      DisableBit(ctrlReg, infoIdx + 2);
      EnableBit(ctrlReg, infoIdx + 3);
      break;
    default:
      DS2LOG(Error, "invalid hardware breakpoint size: %d", size);
      return kErrorInvalidArgument;
    }
  }

  return kSuccess;
}

ErrorCode HardwareBreakpointManager::disableDebugCtrlReg(uint32_t &ctrlReg,
                                                         int idx) {
  // Unset G<idx> flag
  DisableBit(ctrlReg, 1 + (idx * 2));

  return kSuccess;
}

int HardwareBreakpointManager::getAvailableLocation() {
  DS2ASSERT(_locations.size() == kMaxHWStoppoints);
  if (_sites.size() == kMaxHWStoppoints) {
    return -1;
  }

  auto it = std::find(_locations.begin(), _locations.end(), 0);
  DS2ASSERT(it != _locations.end());

  return (it - _locations.begin());
}

int HardwareBreakpointManager::hit(Target::Thread *thread, Site &site) {
  if (_sites.size() == 0) {
    return -1;
  }

  if (thread->state() != Target::Thread::kStopped) {
    return -1;
  }

  uint32_t status_reg = thread->readDebugReg(kStatusRegIdx);

  for (int i = 0; i < kMaxHWStoppoints; ++i) {
    if (status_reg & (1 << i)) {
      DS2ASSERT(_locations[i] != 0);
      site = _sites.find(_locations[i])->second;
      return i;
    }
  }

  return -1;
}

ErrorCode HardwareBreakpointManager::isValid(Address const &address,
                                             size_t size, Mode mode) const {
  switch (size) {
  case 1:
    break;
  case 8:
    DS2LOG(Warning, "8-byte breakpoints not supported on all architectures");
  case 2:
  case 4:
    if (mode == kModeExec)
      return kErrorInvalidArgument;
    break;
  default:
    return kErrorInvalidArgument;
  }

  if ((mode & kModeExec) && (mode & (kModeRead | kModeWrite))) {
    return kErrorInvalidArgument;
  }

  if (mode == kModeRead) {
    return kErrorUnsupported;
  }

  return super::isValid(address, size, mode);
}
}
}
}
