//  Portions Copyright (c) 2023, ApeCloud Inc Holding Limited
// Portions Copyright (c) 2020, Alibaba Group Holding Limited
//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#pragma once

#include <set>

#include "table/internal_iterator.h"

namespace smartengine {

namespace util {
class Arena;
}

namespace table {

// A internal wrapper class with an interface similar to Iterator that caches
// the valid() and key() results for an underlying iterator.
// This can help avoid virtual function calls and also gives better
// cache locality.
class IteratorWrapper {
 public:
  IteratorWrapper() : iter_(nullptr), valid_(false) {}
  explicit IteratorWrapper(InternalIterator* _iter) : iter_(nullptr) {
    Set(_iter);
  }
  IteratorWrapper(const IteratorWrapper &) = default;
  IteratorWrapper &operator=(const IteratorWrapper &) = default;
  ~IteratorWrapper() {}

  InternalIterator* iter() const { return iter_; }

  // Set the underlying Iterator to _iter and return
  // previous underlying Iterator.
  InternalIterator* Set(InternalIterator* _iter) {
    InternalIterator* old_iter = iter_;

    iter_ = _iter;
    if (iter_ == nullptr) {
      valid_ = false;
    } else {
      Update();
    }
    return old_iter;
  }

  void DeleteIter(bool is_arena_mode) {
    if (iter_) {
      if (!is_arena_mode) {
        MOD_DELETE_OBJECT(InternalIterator, iter_);
      } else {
        iter_->~InternalIterator();
      }
    }
  }

  // Iterator interface methods
  bool Valid() const { return valid_; }
  common::Slice key() const {
    assert(Valid());
    return key_;
  }
  common::Slice value() const {
    assert(Valid());
    return iter_->value();
  }
  // Methods below require iter() != nullptr
  common::Status status() const {
    assert(iter_);
    return iter_->status();
  }
  void Next() {
    assert(iter_);
    iter_->Next();
    Update();
  }
  void Prev() {
    assert(iter_);
    iter_->Prev();
    Update();
  }
  void Seek(const common::Slice& k) {
    assert(iter_);
    iter_->Seek(k);
    Update();
  }
  void SeekForPrev(const common::Slice& k) {
    assert(iter_);
    iter_->SeekForPrev(k);
    Update();
  }
  void SeekToFirst() {
    assert(iter_);
    iter_->SeekToFirst();
    Update();
  }
  void SeekToLast() {
    assert(iter_);
    iter_->SeekToLast();
    Update();
  }

  void SetPinnedItersMgr(db::PinnedIteratorsManager* pinned_iters_mgr) {
    assert(iter_);
    iter_->SetPinnedItersMgr(pinned_iters_mgr);
  }
  bool IsKeyPinned() const {
    assert(Valid());
    return iter_->IsKeyPinned();
  }
  bool IsValuePinned() const {
    assert(Valid());
    return iter_->IsValuePinned();
  }

 private:
  void Update() {
    valid_ = iter_->Valid();
    if (valid_) {
      key_ = iter_->key();
    }
  }

  InternalIterator* iter_;
  bool valid_;
  common::Slice key_;
};

// Return an empty iterator (yields nothing) allocated from arena.
extern InternalIterator* NewEmptyInternalIterator(memory::SimpleAllocator* arena);

// Return an empty iterator with the specified status, allocated arena.
extern InternalIterator* NewErrorInternalIterator(const common::Status& status,
                                                  memory::SimpleAllocator* arena);

}  // namespace table
}  // namespace smartengine
