//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#ifndef __DebugServer2_Target_Linux_Thread_h
#define __DebugServer2_Target_Linux_Thread_h

#include "DebugServer2/Target/POSIX/Thread.h"

#include <csignal>

namespace ds2 {
namespace Target {
namespace Linux {

class Thread : public ds2::Target::POSIX::Thread {
protected:
  friend class Process;
  Thread(Process *process, ThreadId tid);

#if defined(ARCH_X86) || defined(ARCH_X86_64)
public:
  uintptr_t readDebugReg(size_t idx) const override;
  ErrorCode writeDebugReg(size_t idx, uintptr_t val) const override;
#endif

protected:
  void fillWatchpointData();

protected:
  ErrorCode updateStopInfo(int waitStatus) override;
  void updateState() override;
};
}
}
}

#endif // !__DebugServer2_Target_Linux_Thread_h
