//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#define __DS2_LOG_CLASS_NAME__ "Target::Thread"

#include "DebugServer2/Target/Thread.h"
#include "DebugServer2/Architecture/X86/RegisterCopy.h"
#include "DebugServer2/Host/Platform.h"
#include "DebugServer2/Host/Windows/ExtraWrappers.h"
#include "DebugServer2/Utils/Log.h"

#include <windows.h>

using ds2::Host::Platform;

namespace ds2 {
namespace Target {
namespace Windows {

ErrorCode Thread::step(int signal, Address const &address) {
  // Windows doesn't have a dedicated single-step call. We have to set the
  // single step flag (TF, 8th bit) in eflags and resume the thread.
  ErrorCode error = modifyRegisters(
      [](Architecture::CPUState &state) { state.gp.eflags |= (1 << 8); });
  if (error != kSuccess) {
    return error;
  }
  return resume(signal, address);
}

ErrorCode Thread::readCPUState(Architecture::CPUState &state) {
  CONTEXT context;

  std::memset(&context, 0, sizeof(context));
  // TODO(sas): Handle AVX.
  context.ContextFlags = CONTEXT_INTEGER |            // GP registers.
                         CONTEXT_CONTROL |            // Some more GP + cs/ss.
                         CONTEXT_SEGMENTS |           // Data segment selectors.
                         CONTEXT_FLOATING_POINT |     // FP registers.
                         CONTEXT_EXTENDED_REGISTERS | // SSE registers.
                         CONTEXT_DEBUG_REGISTERS;     // Debug registers.

  BOOL result = GetThreadContext(_handle, &context);
  if (!result) {
    return Platform::TranslateError();
  }

  user_to_state32(state, context);

  // x87 state
  state.x87.fstw = context.FloatSave.StatusWord;
  state.x87.fctw = context.FloatSave.ControlWord;
  state.x87.ftag = context.FloatSave.TagWord;
  state.x87.fiseg = context.FloatSave.ErrorSelector;
  state.x87.fioff = context.FloatSave.ErrorOffset;
  state.x87.foseg = context.FloatSave.DataSelector;
  state.x87.fooff = context.FloatSave.DataOffset;
  // TODO(sas): Figure out where this is stored.
  // state.x87.fop = ???;

  uint8_t const *st_space =
      reinterpret_cast<uint8_t const *>(context.FloatSave.RegisterArea);
  for (size_t n = 0; n < 8; n++) {
    static const int reg_size = sizeof(state.x87.regs[0].bytes);
    memcpy(state.x87.regs[n].bytes, st_space + n * reg_size, reg_size);
  }

  // SSE state
  if (IsProcessorFeaturePresent(PF_XMMI_INSTRUCTIONS_AVAILABLE)) {
    // SSE registers are located at offset 160 and MXCSR is at offset 24 in the
    // ExtendedRegisters block.
    uint8_t const *xmm_space =
        reinterpret_cast<uint8_t const *>(context.ExtendedRegisters);

    memcpy(&state.sse.mxcsr, xmm_space + 24, sizeof(state.sse.mxcsr));
    memcpy(&state.sse.mxcsrmask, xmm_space + 28, sizeof(state.sse.mxcsrmask));
    for (size_t n = 0; n < 8; n++) {
      static const int reg_size = sizeof(state.sse.regs[0]);
      memcpy(&state.sse.regs[n], xmm_space + 160 + n * reg_size, reg_size);
    }
  }

  return kSuccess;
}

ErrorCode Thread::writeCPUState(Architecture::CPUState const &state) {
  CONTEXT context;

  std::memset(&context, 0, sizeof(context));
  // TODO(sas): Handle floats, SSE, AVX and debug registers.
  context.ContextFlags = CONTEXT_INTEGER | // GP registers.
                         CONTEXT_CONTROL | // Some more GP + cs/ss.
                         CONTEXT_SEGMENTS; // Data segment selectors.

  state32_to_user(context, state);

  BOOL result = SetThreadContext(_handle, &context);
  if (!result) {
    return Platform::TranslateError();
  }

  return kSuccess;
}
}
}
}
