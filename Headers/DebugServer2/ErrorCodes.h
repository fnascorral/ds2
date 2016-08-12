//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#ifndef __DebugServer2_ErrorCodes_h
#define __DebugServer2_ErrorCodes_h

#include <type_traits>

namespace ds2 {

// Error Codes as defined by GDB remoting documentation,
// plus some others.
enum ErrorCode {
  kSuccess,
  kErrorNoPermission = 1,
  kErrorNotFound = 2,
  kErrorProcessNotFound = 3,
  kErrorInterrupted = 4,
  kErrorInvalidHandle = 9,
  kErrorNoMemory = 12,
  kErrorAccessDenied = 13,
  kErrorInvalidAddress = 14,
  kErrorBusy = 16,
  kErrorAlreadyExist = 17,
  kErrorNoDevice = 19,
  kErrorNotDirectory = 20,
  kErrorIsDirectory = 21,
  kErrorInvalidArgument = 22,
  kErrorTooManySystemFiles = 23,
  kErrorTooManyFiles = 24,
  kErrorFileTooBig = 27,
  kErrorNoSpace = 28,
  kErrorInvalidSeek = 29,
  kErrorNotWriteable = 30,
  kErrorNameTooLong = 91,
  kErrorUnknown = 9999,
  kErrorUnsupported = 10000
};

char const *GetErrorCodeString(ErrorCode err);

#define CHK_ACTION(C, A)                                                       \
  do {                                                                         \
    static_assert(std::is_same<decltype(C), ErrorCode>::value,                 \
                  #C "does not return an ErrorCode");                          \
    ErrorCode CHK_error = (C);                                                 \
    if (CHK_error != kSuccess) {                                               \
      A;                                                                       \
    }                                                                          \
  } while (0)
}

#define CHK(C) CHK_ACTION(C, return CHK_error)

#endif // !__DebugServer2_ErrorCodes_h
