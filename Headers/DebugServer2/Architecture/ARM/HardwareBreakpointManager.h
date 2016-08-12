//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#ifndef __DebugServer2_Architecture_ARM_HardwareBreakpointManager_h
#define __DebugServer2_Architecture_ARM_HardwareBreakpointManager_h

#include "DebugServer2/BreakpointManager.h"

namespace ds2 {
namespace Architecture {
namespace ARM {

class HardwareBreakpointManager : public BreakpointManager {
public:
  HardwareBreakpointManager(Target::ProcessBase *process);
  ~HardwareBreakpointManager() override;

public:
  ErrorCode add(Address const &address, Type type, size_t size,
                Mode mode) override {
    return kErrorUnsupported;
  };

public:
  int hit(Target::Thread *thread, Site &site) override { return -1; };

protected:
  ErrorCode enableLocation(Site const &site) override {
    return kErrorUnsupported;
  };

  ErrorCode disableLocation(Site const &site) override {
    return kErrorUnsupported;
  };

public:
  virtual int maxWatchpoints();

private:
  int _maxBreakpoints;
  int _maxWatchpoints;
  int _maxWatchpointSize;
};
}
}
}

#endif // !__DebugServer2_Architecture_ARM_HardwareBreakpointManager_h
