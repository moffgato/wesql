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
#include <stdint.h>
#include <memory>
#include "db/log_format.h"
#include "options/options.h"
#include "util/aio_wrapper.h"

namespace smartengine {
namespace util {
class SequentialFileReader;
}

namespace db {

using std::unique_ptr;

namespace log {

/**
 * Reader is a general purpose log stream reader implementation. The actual job
 * of reading from the device is implemented by the util::SequentialFile
 * interface.
 *
 * Please see Writer for details on the file and record layout.
 */
class Reader {
 public:
  // Interface for reporting errors.
  class Reporter {
   public:
    virtual ~Reporter();

    // Some corruption was detected.  "size" is the approximate number
    // of bytes dropped due to the corruption.
    virtual void Corruption(size_t bytes, const common::Status& status) = 0;
  };

  // Create a reader that will return log records from "*file".
  // "*file" must remain live while this Reader is in use.
  //
  // If "reporter" is non-nullptr, it is notified whenever some data is
  // dropped due to a detected corruption.  "*reporter" must remain
  // live while this Reader is in use.
  //
  // If "checksum" is true, verify checksums if available.
  //
  // The Reader will start reading at the first record located at physical
  // position >= initial_offset within the file.
  Reader(util::SequentialFileReader *file, Reporter* reporter,
         bool checksum, uint64_t initial_offset, uint64_t log_number,
         bool use_allocator = false, bool use_aio = false, uint64_t file_size = 0);

  ~Reader();

  // Read the next record into *record.  Returns true if read
  // successfully, false if we hit end of the input.  May use
  // "*scratch" as temporary storage.  The contents filled in *record
  // will only be valid until the next mutating operation on this
  // reader or the next mutation to *scratch.
  // If record_crc is not null then this method won't check records's checksum
  // and return the record_crc when log_reader's checksum_ member is true
  bool ReadRecord(common::Slice* record, std::string* scratch,
                  common::WALRecoveryMode wal_recovery_mode =
                      common::WALRecoveryMode::kTolerateCorruptedTailRecords,
                  uint32_t* record_crc = nullptr);

  // Returns the physical offset of the last record returned by ReadRecord.
  //
  // Undefined before the first call to ReadRecord.
  uint64_t LastRecordOffset();

  //return the physical end pos of last record returned by ReadRecord
  uint64_t get_last_record_end_pos() { return last_record_end_pos_; }

  // returns true if the reader has encountered an eof condition.
  bool IsEOF() { return eof_; }

  // returns true if the record returned by ReadRecord is the last record
  bool is_real_eof();

  // when we know more data has been written to the file. we can use this
  // function to force the reader to look again in the file.
  // Also aligns the file position indicator to the start of the next block
  // by reading the rest of the data from the EOF position to the end of the
  // block that was partially read.
  void UnmarkEOF();

  const util::SequentialFileReader* file() { return file_; }
  util::SequentialFileReader* internal_get_file() { return file_; }
  util::SequentialFileReader* release_file() {
    auto rfile = file_;
    file_ = nullptr;
    return rfile;
  }

  void delete_file(memory::SimpleAllocator *arena);
 private:
  util::SequentialFileReader *file_;
  Reporter* const reporter_;
  bool const checksum_;
  char* const backing_store_;
  common::Slice buffer_;
  bool eof_;         // Last Read() indicated EOF by returning < kBlockSize
  bool read_error_;  // Error occurred while reading from file

  // Offset of the file position indicator within the last block when an
  // EOF was detected.
  size_t eof_offset_;

  // Offset of the last record returned by ReadRecord.
  uint64_t last_record_offset_;
  //end pos of the last record returned by ReadRecord
  uint64_t last_record_end_pos_;
  // Offset of the first location past the end of buffer_.
  uint64_t end_of_buffer_offset_;

  // Offset at which to start looking for the first record to return
  uint64_t const initial_offset_;

  // which log number this is
  uint64_t const log_number_;

  // Whether this is a recycled log file
  bool recycled_;

  bool use_allocator_;

  // For aio read
  bool use_aio_;
  bool aio_error_;
  static const int32_t MAX_PREFETCH_LOG_BLOCK_NUM = 48;
  util::AIOHandle aio_handles_[MAX_PREFETCH_LOG_BLOCK_NUM];
  int aio_read_index_;
  uint64_t const file_size_;

  // Extend record types with the following special values
  enum {
    kEof = kMaxRecordType + 1,
    // Returned whenever we find an invalid physical record.
    // Currently there are three situations in which this happens:
    // * The record has an invalid CRC (ReadPhysicalRecord reports a drop)
    // * The record is a 0-length record (No drop is reported)
    // * The record is below constructor's initial_offset (No drop is reported)
    kBadRecord = kMaxRecordType + 2,
    // Returned when we fail to read a valid header.
    kBadHeader = kMaxRecordType + 3,
    // Returned when we read an old record from a previous user of the log.
    kOldRecord = kMaxRecordType + 4,
    // Returned when we get a bad record length
    kBadRecordLen = kMaxRecordType + 5,
    // Returned when we get a bad record checksum
    kBadRecordChecksum = kMaxRecordType + 6,
  };

  // Skips all blocks that are completely before "initial_offset_".
  //
  // Returns true on success. Handles reporting.
  bool SkipToInitialBlock();

  // Return type, or one of the preceding special values
  unsigned int ReadPhysicalRecord(common::Slice* result, size_t* drop_size,
                                  uint32_t* record_crc);

  // Read some more
  bool ReadMore(size_t* drop_size, int* error);

  // Reports dropped bytes to the reporter.
  // buffer_ must be updated to remove the dropped bytes prior to invocation.
  void ReportCorruption(size_t bytes, const char* reason);
  void ReportDrop(size_t bytes, const common::Status& reason);

  int check_and_prefetch();

  // No copying allowed
  Reader(const Reader&);
  void operator=(const Reader&);
};

}  // namespace log
}
}  // namespace smartengine
