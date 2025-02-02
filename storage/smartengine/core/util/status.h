/*
 * Portions Copyright (c) 2023, ApeCloud Inc Holding Limited
 * Portions Copyright (c) 2020, Alibaba Group Holding Limited
 */
// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// A Status encapsulates the result of an operation.  It may indicate success,
// or it may indicate an error with an associated error message.
//
// Multiple threads can invoke const methods on a Status without
// external synchronization, but if any of the threads may call a
// non-const method, all threads accessing the same Status must use
// external synchronization.

#ifndef STORAGE_ROCKSDB_INCLUDE_STATUS_H_
#define STORAGE_ROCKSDB_INCLUDE_STATUS_H_

#include <string>
#include "util/slice.h"

namespace smartengine {
namespace common {

class Status {
 public:
  enum Code {
    kOk = 0,
    kNotFound = 1,
    kCorruption = 2,
    kNotSupported = 3,
    kInvalidArgument = 4,
    kIOError = 5,
    kMergeInProgress = 6,
    kIncomplete = 7,
    kShutdownInProgress = 8,
    kTimedOut = 9,
    kAborted = 10,
    kBusy = 11,
    kExpired = 12,
    kTryAgain = 13,
    kMutexTimeout = 14,
    kLockTimeout = 15,
    kLockLimit = 16,
    kNoSpace = 17,
    kDeadlock = 18,
    kStaleFile = 19,
    kMemoryLimit = 20,
    kErrorUnexpected = 21,
    kEntryNotExist = 22,
    kInitTwice = 23,
    kNotInit = 24,
    kIterEnd = 25,
    kCancelTask = 26,
    kInsertCheckFailed = 27,
    kOverLimit = 28,
    kRecoverCommitLater = 29,
    kObjStoreError = 30,
    kNotCompress = 31,
  };

  enum SubCode { kNone = 0, kMaxSubCode };

  // Create a success status.
  Status() : code_(kOk), subcode_(kNone), state_(nullptr) {}
  explicit Status(Code _code, SubCode _subcode = kNone)
      : code_(_code), subcode_(_subcode), state_(nullptr) {}

  Status(Code _code, SubCode _subcode, const Slice &msg, const Slice &msg2);
  Status(Code _code, const Slice &msg, const Slice &msg2)
      : Status(_code, kNone, msg, msg2) {}
  ~Status() { delete[] state_; }

  // Copy the specified status.
  Status(const Status &s);
  Status &operator=(const Status &s);
  Status(Status &&s)
#if !(defined _MSC_VER) || ((defined _MSC_VER) && (_MSC_VER >= 1900))
      noexcept
#endif
      ;
  Status &operator=(Status &&s)
#if !(defined _MSC_VER) || ((defined _MSC_VER) && (_MSC_VER >= 1900))
      noexcept
#endif
      ;
  bool operator==(const Status &rhs) const;
  bool operator!=(const Status &rhs) const;

  Code code() const { return code_; }

  SubCode subcode() const { return subcode_; }

  Status(int _code)
      : code_(static_cast<Code>(_code)), subcode_(kNone), state_(nullptr) {}

  // Returns a C style string indicating the message of the Status
  const char *getState() const { return state_; }

  // Return a success status.
  static Status OK() { return Status(); }

  // Return error status of an appropriate type.
  static Status NotFound(const Slice &msg, const Slice &msg2 = Slice()) {
    return Status(kNotFound, msg, msg2);
  }
  // Fast path for not found without malloc;
  static Status NotFound(SubCode msg = kNone) { return Status(kNotFound, msg); }

  static Status Corruption(const Slice &msg, const Slice &msg2 = Slice()) {
    return Status(kCorruption, msg, msg2);
  }
  static Status Corruption(SubCode msg = kNone) {
    return Status(kCorruption, msg);
  }

  static Status NotSupported(const Slice &msg, const Slice &msg2 = Slice()) {
    return Status(kNotSupported, msg, msg2);
  }
  static Status NotSupported(SubCode msg = kNone) {
    return Status(kNotSupported, msg);
  }

  static Status NotInited(const Slice &msg, const Slice &msg2 = Slice()) { return Status(kNotInit, msg, msg2); }

  static Status NotInited(SubCode msg = kNone) { return Status(kNotInit, msg); }

  static Status InvalidArgument(const Slice &msg, const Slice &msg2 = Slice()) {
    return Status(kInvalidArgument, msg, msg2);
  }
  static Status InvalidArgument(SubCode msg = kNone) {
    return Status(kInvalidArgument, msg);
  }

  static Status IOError(const Slice &msg, const Slice &msg2 = Slice()) {
    return Status(kIOError, msg, msg2);
  }
  static Status IOError(SubCode msg = kNone) { return Status(kIOError, msg); }

  static Status MergeInProgress(const Slice &msg, const Slice &msg2 = Slice()) {
    return Status(kMergeInProgress, msg, msg2);
  }
  static Status MergeInProgress(SubCode msg = kNone) {
    return Status(kMergeInProgress, msg);
  }

  static Status Incomplete(const Slice &msg, const Slice &msg2 = Slice()) {
    return Status(kIncomplete, msg, msg2);
  }
  static Status Incomplete(SubCode msg = kNone) {
    return Status(kIncomplete, msg);
  }

  static Status ShutdownInProgress(SubCode msg = kNone) {
    return Status(kShutdownInProgress, msg);
  }
  static Status ShutdownInProgress(const Slice &msg,
                                   const Slice &msg2 = Slice()) {
    return Status(kShutdownInProgress, msg, msg2);
  }
  static Status Aborted(SubCode msg = kNone) { return Status(kAborted, msg); }
  static Status Aborted(const Slice &msg, const Slice &msg2 = Slice()) {
    return Status(kAborted, msg, msg2);
  }

  static Status Busy(SubCode msg = kNone) { return Status(kBusy, msg); }
  static Status Busy(const Slice &msg, const Slice &msg2 = Slice()) {
    return Status(kBusy, msg, msg2);
  }

  static Status TimedOut(SubCode msg = kNone) { return Status(kTimedOut, msg); }
  static Status TimedOut(const Slice &msg, const Slice &msg2 = Slice()) {
    return Status(kTimedOut, msg, msg2);
  }

  static Status Expired(SubCode msg = kNone) { return Status(kExpired, msg); }
  static Status Expired(const Slice &msg, const Slice &msg2 = Slice()) {
    return Status(kExpired, msg, msg2);
  }

  static Status TryAgain(SubCode msg = kNone) { return Status(kTryAgain, msg); }
  static Status TryAgain(const Slice &msg, const Slice &msg2 = Slice()) {
    return Status(kTryAgain, msg, msg2);
  }

  static Status NoSpace() { return Status(kNoSpace); }
  static Status NoSpace(const Slice &msg, const Slice &msg2 = Slice()) {
    return Status(kNoSpace, msg, msg2);
  }

  static Status MemoryLimit() { return Status(kMemoryLimit); }
  static Status MemoryLimit(const Slice &msg, const Slice &msg2 = Slice()) {
    return Status(kMemoryLimit, msg, msg2);
  }

  static Status RecoverCommitLater() { return Status(kRecoverCommitLater); }
  static Status RecoverCommitLater(const Slice &msg,
                                   const Slice &msg2 = Slice()) {
    return Status(kRecoverCommitLater, msg, msg2);
  }

  // Returns true iff the status indicates success.
  bool ok() const { return code() == kOk; }

  // Returns true iff the status indicates a NotFound error.
  bool IsNotFound() const { return code() == kNotFound; }

  // Returns true iff the status indicates a Corruption error.
  bool IsCorruption() const { return code() == kCorruption; }

  // Returns true iff the status indicates a NotSupported error.
  bool IsNotSupported() const { return code() == kNotSupported; }

  // Returns true iff the status indicates a NotInited error.
  bool IsNotInited() const { return code() == kNotInit; }

  // Returns true iff the status indicates an InvalidArgument error.
  bool IsInvalidArgument() const { return code() == kInvalidArgument; }

  // Returns true iff the status indicates an IOError.
  bool IsIOError() const { return code() == kIOError; }

  // Returns true iff the status indicates an MergeInProgress.
  bool IsMergeInProgress() const { return code() == kMergeInProgress; }

  // Returns true iff the status indicates Incomplete
  bool IsIncomplete() const { return code() == kIncomplete; }

  // Returns true iff the status indicates Shutdown In progress
  bool IsShutdownInProgress() const { return code() == kShutdownInProgress; }

  bool IsTimedOut() const { return code() == kTimedOut; }

  bool IsAborted() const { return code() == kAborted; }

  bool IsLockLimit() const { return code() == kLockLimit; }

  // Returns true iff the status indicates that a resource is Busy and
  // temporarily could not be acquired.
  bool IsBusy() const { return code() == kBusy; }

  bool IsDeadlock() const { return code() == kDeadlock; }

  // Returns true iff the status indicated that the operation has Expired.
  bool IsExpired() const { return code() == kExpired; }

  // Returns true iff the status indicates a TryAgain error.
  // This usually means that the operation failed, but may succeed if
  // re-attempted.
  bool IsTryAgain() const { return code() == kTryAgain; }

  // Returns true iff the status indicates a NoSpace error
  // This is caused by an I/O error returning the specific "out of space"
  // error condition. Stricto sensu, an NoSpace error is an I/O error
  // with a specific subcode, enabling users to take the appropriate action
  // if needed
  bool IsNoSpace() const { return code() == kNoSpace; }

  // Returns true iff the status indicates a memory limit error.  There may be
  // cases where we limit the memory used in certain operations (eg. the size
  // of a write batch) in order to avoid out of memory exceptions.
  bool IsMemoryLimit() const { return code() == kMemoryLimit; }

  // Only useful during wal recovering
  // Returns true if current record is not prepared and will be committed later
  bool is_recover_commit_later() const { return code() == kRecoverCommitLater; }

  // Return a string representation of this status suitable for printing.
  // Returns the string "OK" for success.
  std::string ToString() const;

 private:
  // A nullptr state_ (which is always the case for OK) means the message
  // is empty.
  // of the following form:
  //    state_[0..3] == length of message
  //    state_[4..]  == message
  Code code_;
  SubCode subcode_;
  const char *state_;

  static const char *msgs[static_cast<int>(kMaxSubCode)];

  static const char *CopyState(const char *s);
};

inline Status::Status(const Status &s) : code_(s.code_), subcode_(s.subcode_) {
  state_ = (s.state_ == nullptr) ? nullptr : CopyState(s.state_);
}
inline Status &Status::operator=(const Status &s) {
  // The following condition catches both aliasing (when this == &s),
  // and the common case where both s and *this are ok.
  if (this != &s) {
    code_ = s.code_;
    subcode_ = s.subcode_;
    delete[] state_;
    state_ = (s.state_ == nullptr) ? nullptr : CopyState(s.state_);
  }
  return *this;
}

inline Status::Status(Status &&s)
#if !(defined _MSC_VER) || ((defined _MSC_VER) && (_MSC_VER >= 1900))
    noexcept
#endif
    : Status() {
  *this = std::move(s);
}

inline Status &Status::operator=(Status &&s)
#if !(defined _MSC_VER) || ((defined _MSC_VER) && (_MSC_VER >= 1900))
    noexcept
#endif
{
  if (this != &s) {
    code_ = std::move(s.code_);
    s.code_ = kOk;
    subcode_ = std::move(s.subcode_);
    s.subcode_ = kNone;
    delete[] state_;
    state_ = nullptr;
    std::swap(state_, s.state_);
  }
  return *this;
}

inline bool Status::operator==(const Status &rhs) const {
  return (code_ == rhs.code_);
}

inline bool Status::operator!=(const Status &rhs) const {
  return !(*this == rhs);
}

}  // namespace common
}  // namespace smartengine

#endif  // STORAGE_ROCKSDB_INCLUDE_STATUS_H_
