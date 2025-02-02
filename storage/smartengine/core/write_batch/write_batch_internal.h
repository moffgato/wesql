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
#include <vector>
#include "db/batch_group.h"
#include "db/db.h"
#include "options/options.h"
#include "util/autovector.h"
#include "write_batch/write_batch.h"

namespace smartengine {
namespace db {

class MemTable;
class FlushScheduler;
class ColumnFamilyData;

class ColumnFamilyMemTables {
 public:
  virtual ~ColumnFamilyMemTables() {}
  virtual bool Seek(uint32_t column_family_id) = 0;
  // returns true if the update to memtable should be ignored
  // (useful when recovering from log whose updates have already
  // been processed)
  virtual uint64_t GetLogNumber() const = 0;
  virtual common::SequenceNumber GetSequence() const = 0;
  virtual MemTable* GetMemTable() const = 0;
  virtual ColumnFamilyHandle* GetColumnFamilyHandle() = 0;
  virtual ColumnFamilyData* current() { return nullptr; }
  virtual bool is_subtable_dropped(int64_t index_id)
  {
    UNUSED(index_id);
    return false;
  }
};

class ColumnFamilyMemTablesDefault : public ColumnFamilyMemTables {
 public:
  explicit ColumnFamilyMemTablesDefault(MemTable* mem)
      : ok_(false), mem_(mem) {}

  bool Seek(uint32_t column_family_id) override {
    ok_ = (column_family_id == 0);
    return ok_;
  }

  uint64_t GetLogNumber() const override { return 0; }

  common::SequenceNumber GetSequence() const override {return 0; }
  MemTable* GetMemTable() const override {
    assert(ok_);
    return mem_;
  }

  ColumnFamilyHandle* GetColumnFamilyHandle() override { return nullptr; }

 private:
  bool ok_;
  MemTable* mem_;
};

// WriteBatchInternal provides static methods for manipulating a
// WriteBatch that we don't want in the public WriteBatch interface.
class WriteBatchInternal {
 public:
  // WriteBatch header has an 8-byte sequence number followed by a 4-byte count.
  static const size_t kHeader = 12;

  // WriteBatch methods with column_family_id instead of ColumnFamilyHandle*
  static common::Status Put(WriteBatch* batch, uint32_t column_family_id,
                            const common::Slice& key,
                            const common::Slice& value);

  static common::Status Put(WriteBatch* batch, uint32_t column_family_id,
                            const common::SliceParts& key,
                            const common::SliceParts& value);

  static common::Status Delete(WriteBatch* batch, uint32_t column_family_id,
                               const common::SliceParts& key);

  static common::Status Delete(WriteBatch* batch, uint32_t column_family_id,
                               const common::Slice& key);

  static common::Status SingleDelete(WriteBatch* batch,
                                     uint32_t column_family_id,
                                     const common::SliceParts& key);

  static common::Status SingleDelete(WriteBatch* batch,
                                     uint32_t column_family_id,
                                     const common::Slice& key);

  static common::Status MarkBeginPrepare(WriteBatch* batch);

  static common::Status MarkEndPrepare(WriteBatch* batch,
                                       const common::Slice& xid);

  static common::Status MarkEndPrepare(WriteBatch* batch,
                                       const common::Slice& xid,
                                       common::SequenceNumber prepare_seq);

  static common::Status MarkRollback(WriteBatch* batch,
                                     const common::Slice& xid);

  static common::Status MarkRollback(WriteBatch* batch,
                                     const common::Slice& xid,
                                     common::SequenceNumber prepare_seq);

  static common::Status MarkCommit(WriteBatch* batch, const common::Slice& xid);

  static common::Status MarkCommit(WriteBatch* batch,
                                   const common::Slice& xid,
                                   common::SequenceNumber prepare_seq);

  static common::Status InsertNoop(WriteBatch* batch);

  // Return the number of entries in the batch.
  static int Count(const WriteBatch* batch);

  // Set the count for the number of entries in the batch.
  static void SetCount(WriteBatch* batch, int n);

  // Return the seqeunce number for the start of this batch.
  static common::SequenceNumber Sequence(const WriteBatch* batch);

  // Store the specified number as the seqeunce number for the start of
  // this batch.
  static void SetSequence(WriteBatch* batch, common::SequenceNumber seq);

  // Returns the offset of the first entry in the batch.
  // This offset is only valid if the batch is not empty.
  static size_t GetFirstOffset(WriteBatch* batch);

  static common::Slice Contents(const WriteBatch* batch) {
    return common::Slice(batch->rep_);
  }

  static size_t ByteSize(const WriteBatch* batch) { return batch->rep_.size(); }

  static common::Status SetContents(WriteBatch* batch,
                                    const common::Slice& contents);

  // Convenience form of InsertInto when you have only one batch
  // last_seq_used returns the last sequnce number used in a MemTable insert
  static common::Status InsertInto(
      const WriteBatch* batch, ColumnFamilyMemTables* memtables,
      FlushScheduler* flush_scheduler,
      bool ignore_missing_column_families = false, uint64_t log_number = 0,
      DB* db = nullptr, bool concurrent_memtable_writes = false,
      common::SequenceNumber* last_seq_used = nullptr,
      bool* has_valid_writes = nullptr, std::unordered_map<int64_t, int64_t> *missing_subtable_during_recovery = nullptr);

  // for pipline insert in serialization mode
  static common::Status InsertInto(db::WriteRequest* writer,
                                   common::SequenceNumber sequence,
                                   ColumnFamilyMemTables* memtables,
                                   FlushScheduler* flush_scheduler,
                                   bool ignore_missing_column_families = false,
                                   uint64_t log_number = 0, DB* db = nullptr,
                                   bool concurrent_memtable_writes = false);
  // for pipline insert in parallel mode
  static common::Status InsertInto(db::WriteRequest* writer,
                                   ColumnFamilyMemTables* memtables,
                                   FlushScheduler* flush_scheduler,
                                   bool ignore_missing_column_families = false,
                                   uint64_t log_number = 0, DB* db = nullptr,
                                   bool concurrent_memtable_writes = true);

  static common::Status Append(WriteBatch* dst, const WriteBatch* src,
                               const bool WAL_only = false);

  // Returns the byte size of appending a WriteBatch with ByteSize
  // leftByteSize and a WriteBatch with ByteSize rightByteSize
  static size_t AppendedByteSize(size_t leftByteSize, size_t rightByteSize);
};

// LocalSavePoint is similar to a scope guard
class LocalSavePoint {
 public:
  explicit LocalSavePoint(WriteBatch* batch)
      : batch_(batch),
        savepoint_(batch->GetDataSize(), batch->Count(),
                   batch->content_flags_.load(std::memory_order_relaxed))
#ifndef NDEBUG
        ,
        committed_(false)
#endif
  {
  }

#ifndef NDEBUG
  ~LocalSavePoint() { assert(committed_); }
#endif
  common::Status commit() {
#ifndef NDEBUG
    committed_ = true;
#endif
    if (batch_->max_bytes_ && batch_->rep_.size() > batch_->max_bytes_) {
      batch_->rep_.resize(savepoint_.size);
      WriteBatchInternal::SetCount(batch_, savepoint_.count);
      batch_->content_flags_.store(savepoint_.content_flags,
                                   std::memory_order_relaxed);
      return common::Status::MemoryLimit();
    }
    return common::Status::OK();
  }

 private:
  WriteBatch* batch_;
  SavePoint savepoint_;
#ifndef NDEBUG
  bool committed_;
#endif
};
}
}  // namespace smartengine
