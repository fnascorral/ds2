//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#include "DebugServer2/BreakpointManager.h"
#include "DebugServer2/Utils/Log.h"

namespace ds2 {

BreakpointManager::BreakpointManager(Target::ProcessBase *process)
    : _enabled(false), _process(process) {}

BreakpointManager::~BreakpointManager() {
  // cannot call clear() here
}

void BreakpointManager::clear() { _sites.clear(); }

ErrorCode BreakpointManager::add(Address const &address, Type type, size_t size,
                                 Mode mode) {
  CHK(isValid(address, size, mode));

  auto it = _sites.find(address);
  if (it != _sites.end()) {
    if (it->second.mode != mode)
      return kErrorInvalidArgument;

    it->second.type = static_cast<Type>(it->second.type | type);
    if (type == kTypePermanent)
      ++it->second.refs;
  } else {
    Site &site = _sites[address];

    site.refs = 0;
    if (type == kTypePermanent)
      ++site.refs;
    site.address = address;
    site.type = type;
    site.mode = mode;
    site.size = size;

    // If the breakpoint manager is already in enabled state, enable
    // the newly added breakpoint too.
    if (_enabled) {
      return enableLocation(site);
    }
  }

  return kSuccess;
}

ErrorCode BreakpointManager::remove(Address const &address) {
  ErrorCode error = kSuccess;

  if (!address.valid())
    return kErrorInvalidArgument;

  auto it = _sites.find(address);
  if (it == _sites.end())
    return kErrorNotFound;

  // If we have some sort of temporary breakpoint, just remove it.
  if (!(it->second.type & kTypePermanent))
    goto do_remove;

  // If we have a premanent breakpoint, refs should be non-null. If refs is
  // still non-null after the decrement, do not remove the breakpoint and
  // return.
  DS2ASSERT(it->second.refs > 0);
  if (--it->second.refs > 0)
    return kSuccess;

  // If we had a breakpoint that was *only* of type kTypePermanent and refs is
  // now 0, we need to remove it.
  if (it->second.type == kTypePermanent)
    goto do_remove;

  // Otherwise, we had multiple breakpoint types; unset kTypePermanent and
  // return.
  it->second.type = static_cast<Type>(it->second.type & ~kTypePermanent);
  return kSuccess;

do_remove:
  DS2ASSERT(it->second.refs == 0);
  //
  // If the breakpoint manager is already in enabled state, disable
  // the newly removed breakpoint too.
  //
  if (_enabled)
    error = disableLocation(it->second);
  _sites.erase(it);
  return error;
}

bool BreakpointManager::has(Address const &address) const {
  if (!address.valid())
    return false;

  return (_sites.find(address) != _sites.end());
}

void BreakpointManager::enumerate(
    std::function<void(Site const &)> const &cb) const {
  for (auto const &it : _sites) {
    cb(it.second);
  }
}

void BreakpointManager::enable() {
  //
  // Both callbacks should be installed by child class before
  // enabling the breakpoint manager.
  //
  if (_enabled)
    DS2LOG(Warning, "double-enabling breakpoints");
  _enabled = true;

  enumerate([this](Site const &site) { enableLocation(site); });
}

void BreakpointManager::disable() {
  if (!_enabled)
    DS2LOG(Warning, "double-disabling breakpoints");
  _enabled = false;

  enumerate([this](Site const &site) { disableLocation(site); });

  //
  // Remove temporary breakpoints.
  //
  auto it = _sites.begin();
  while (it != _sites.end()) {
    it->second.type =
        static_cast<Type>(it->second.type & ~kTypeTemporaryOneShot);
    if (!it->second.type) {
      // refs should always be 0 unless we have a kTypePermanent breakpoint.
      DS2ASSERT(it->second.refs == 0);
      _sites.erase(it++);
    } else {
      it++;
    }
  }
}

bool BreakpointManager::hit(Address const &address, Site &site) {
  if (!address.valid())
    return false;

  auto it = _sites.find(address);
  if (it == _sites.end())
    return false;

  //
  // If this breakpoint type becomes 0, it will be erased after calling
  // BreakpointManager::disable
  //
  it->second.type =
      static_cast<Type>(it->second.type & ~kTypeTemporaryUntilHit);

  site = it->second;
  return true;
}

ErrorCode BreakpointManager::isValid(Address const &address, size_t size,
                                     Mode mode) const {
  if (!address.valid()) {
    return kErrorInvalidArgument;
  }

  return kSuccess;
}
}
