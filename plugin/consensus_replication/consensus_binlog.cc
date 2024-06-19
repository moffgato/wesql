/* Copyright (c) 2018, 2021, Alibaba and/or its affiliates. All rights reserved.
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.
   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL/Apsara GalaxyEngine hereby grant you an
   additional permission to link the program and your derivative works with the
   separately licensed software that they have included with
   MySQL/Apsara GalaxyEngine.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>

#include "consensus_binlog.h"
#include "consensus_log_index.h"
#include "consensus_log_manager.h"
#include "consensus_recovery_manager.h"
#include "rpl_consensus.h"
#include "system_variables.h"

#include "my_config.h"
#include "my_dbug.h"

#include "sql/log.h"
#include "my_loglevel.h"
#include "mysql/components/services/log_builtins.h"

#include "libbinlogevents/include/binlog_event.h"
#include "my_byteorder.h"
#include "sql/binlog_istream.h"
#include "sql/binlog_reader.h"
#include "sql/debug_sync.h"  // DEBUG_SYNC
#include "sql/log.h"
#include "sql/protocol.h"
#include "sql/sql_lex.h"
#include "sql/sql_class.h"
#include "sql/tztime.h"  // my_tz_SYSTEM
#include "mysql/psi/mysql_file.h"

using binary_log::checksum_crc32;

static int get_lower_bound_pos_of_index(const uint64 start_index,
                                        const uint64 consensus_index,
                                        uint64 &pos, bool &matched);

/**
  Auxiliary class to copy serialized events to the binary log and
  correct some of the fields that are not known until just before
  writing the event.

  The following fields are fixed before writing the event:
  - end_log_pos is set
  - the checksum is computed if checksums are enabled
  - the length is incremented by the checksum size if checksums are enabled
*/
class Consensuslog_event_writer : public Basic_ostream {
  Basic_ostream *m_ostream;
  bool have_checksum;
  bool have_footer;
  bool write_ostream;
  ha_checksum initial_checksum;
  ha_checksum checksum;
  uint32 end_log_pos;
  uchar header[LOG_EVENT_HEADER_LEN];
  my_off_t header_len = 0;
  uint32 event_len = 0;

 public:
  /**
    Constructs a new Binlog_event_writer. Should be called once before
    starting to flush the transaction or statement cache to the
    binlog.

    @param binlog_file to write to.
  */
  Consensuslog_event_writer(Basic_ostream *ostream, bool have_checksum,
                            uint32 end_log_pos, bool write_ostream = true,
                            bool have_footer = false)
      : m_ostream(ostream),
        have_checksum(have_checksum),
        have_footer(have_footer),
        write_ostream(write_ostream),
        initial_checksum(my_checksum(0L, nullptr, 0)),
        checksum(initial_checksum),
        end_log_pos(end_log_pos) {
    if (DBUG_EVALUATE_IF("fault_injection_crc_value", 1, 0)) checksum--;
  }

  void update_header(uchar *header_ptr) {
    event_len = uint4korr(header_ptr + EVENT_LEN_OFFSET);

    // Increase end_log_pos
    end_log_pos += event_len;

    // Update event length if it has checksum
    if (have_checksum && !have_footer) {
      int4store(header_ptr + EVENT_LEN_OFFSET, event_len + BINLOG_CHECKSUM_LEN);
      end_log_pos += BINLOG_CHECKSUM_LEN;
    }

    // Store end_log_pos
    int4store(header_ptr + LOG_POS_OFFSET, end_log_pos);
    // update the checksum
    if (have_checksum) checksum = my_checksum(checksum, header_ptr, header_len);
  }

  bool revise_buffer(uchar *buffer, my_off_t length,
                     uint32 *out_crc = nullptr) {
    DBUG_TRACE;

    assert(!write_ostream && m_ostream == nullptr);
    assert(length > LOG_EVENT_HEADER_LEN);

    while (length > 0) {
      if (event_len == 0) {
        header_len = LOG_EVENT_HEADER_LEN;
        update_header(buffer);

        event_len -= LOG_EVENT_HEADER_LEN;
        length -= LOG_EVENT_HEADER_LEN;
        buffer += LOG_EVENT_HEADER_LEN;
      } else {
        my_off_t scan_bytes = std::min<uint64>(length, event_len);
        bool fill_checksum = false;

        // The whole event will be copied, need fill the checksum
        fill_checksum = (have_checksum && (event_len == scan_bytes));

        // write without checksum
        if (fill_checksum && have_footer) scan_bytes -= BINLOG_CHECKSUM_LEN;

        // update the checksum
        if (have_checksum) checksum = my_checksum(checksum, buffer, scan_bytes);

        // The whole event is copied, now add the checksum
        if (fill_checksum) {
          if (have_footer) {
            int4store(buffer + scan_bytes, checksum);
            scan_bytes += BINLOG_CHECKSUM_LEN;
          }
          if (out_crc) *out_crc = checksum;
          checksum = initial_checksum;
        }

        event_len -= scan_bytes;
        length -= scan_bytes;
        buffer += scan_bytes;
      }
    }
    return false;
  }

  bool write(const uchar *buffer, my_off_t length, uint32 *out_crc) {
    DBUG_TRACE;

    assert(write_ostream && m_ostream != nullptr);

    while (length > 0) {
      /* Write event header into binlog */
      if (event_len == 0) {
        /* data in the buf may be smaller than header size.*/
        uint32 header_incr =
            std::min<uint32>(LOG_EVENT_HEADER_LEN - header_len, length);

        memcpy(header + header_len, buffer, header_incr);
        header_len += header_incr;
        buffer += header_incr;
        length -= header_incr;

        if (header_len == LOG_EVENT_HEADER_LEN) {
          update_header(header);
          if (write_ostream && m_ostream->write(header, header_len))
            return true;

          event_len -= header_len;
          header_len = 0;
        }
      } else {
        my_off_t write_bytes = std::min<uint64>(length, event_len);
        bool write_checksum = false;

        // The whole event will be copied, need add the checksum
        write_checksum = (have_checksum && (event_len == write_bytes));

        // write without checksum
        if (write_checksum && have_footer) write_bytes -= BINLOG_CHECKSUM_LEN;
        if (write_ostream && m_ostream->write(buffer, write_bytes)) return true;

        // update the checksum
        if (have_checksum)
          checksum = my_checksum(checksum, buffer, write_bytes);

        // The whole event is copied, now add the checksum
        if (write_checksum) {
          if (write_ostream) {
            uchar checksum_buf[BINLOG_CHECKSUM_LEN];
            int4store(checksum_buf, checksum);
            if (m_ostream->write(checksum_buf, BINLOG_CHECKSUM_LEN))
              return true;
          }

          if (have_footer) write_bytes += BINLOG_CHECKSUM_LEN;

          if (out_crc) *out_crc = checksum;

          checksum = initial_checksum;
        }

        event_len -= write_bytes;
        length -= write_bytes;
        buffer += write_bytes;
      }
    }
    return false;
  }

  bool write(const uchar *buffer, my_off_t length) override {
    return write(buffer, length, nullptr);
  }

  /**
    Returns true if per event checksum is enabled.
  */
  bool is_checksum_enabled() { return have_checksum; }

  void inc_end_log_pos(uint32 inc) { end_log_pos += inc; }
};

static bool write_consensus_log_event(MYSQL_BIN_LOG::Binlog_ofile *binlog_file,
                                      uint flag, uint64 term, uint64 length,
                                      uint64 checksum,
                                      bool consensus_append = false);

static bool calc_consensus_crc(IO_CACHE_binlog_cache_storage *log_cache,
                               uint32 &crc) {
  uchar *buffer = nullptr;
  my_off_t length = 0;
  DBUG_TRACE;

  crc = binary_log::checksum_crc32(0L, nullptr, 0);

  bool ret = log_cache->begin(&buffer, &length);
  while (!ret && length > 0) {
    binary_log::checksum_crc32(crc, buffer, length);
    ret = log_cache->next(&buffer, &length);
  }
  return ret;
}

int copy_from_consensus_log_cache(IO_CACHE_binlog_cache_storage *from,
                                  uchar *to, my_off_t max_len) {
  uchar *buffer = nullptr;
  my_off_t length = 0;
  my_off_t offset = 0;
  DBUG_TRACE;

  bool ret = from->begin(&buffer, &length);
  while (!ret && length > 0 && offset < max_len) {
    my_off_t to_len = max_len - offset > length ? length : max_len - offset;
    memcpy(to + offset, buffer, to_len);

    offset += to_len;
    if (offset == max_len) break;

    ret = from->next(&buffer, &length);
  }
  assert(offset == max_len);
  return ret;
}

static int large_event_flush(THD *thd, ulonglong total_size, Log_event *ev,
                             ulonglong event_len, ulonglong total_event_len,
                             bool have_checksum,
                             MYSQL_BIN_LOG::Binlog_ofile *binlog_file,
                             ulonglong &total_batch_size,
                             ulonglong &flushed_size) {
  int error = 0;
  uint flag = 0;
  ulonglong ev_footer_size = have_checksum ? BINLOG_CHECKSUM_LEN : 0;
  uint32 ev_crc = 0;
  DBUG_TRACE;

  uint32 batches =
      (total_event_len + opt_consensus_large_event_split_size - 1) /
      opt_consensus_large_event_split_size;

  // more than one batch
  assert(batches > 1);

  /**
   * Use consensus log writer to revise the event, because the end_log_pos is
   * changed
   */
  ulonglong event_start_pos =
      binlog_file_get_current_pos(binlog_file) +
      /* consensus log event for each batch */
      batches * (Consensus_log_event::get_event_length() + ev_footer_size) +
      /* consensus empty log event for non-final batches */
      (batches - 1) *
          (Consensus_empty_log_event::get_event_length() + ev_footer_size);
  Consensuslog_event_writer event_reviser(nullptr, have_checksum,
                                          event_start_pos, false, false);
  event_reviser.revise_buffer((uchar *)ev->temp_buf, event_len, &ev_crc);

  uchar *buffer =
      (uchar *)my_malloc(key_memory_thd_main_mem_root,
                         opt_consensus_large_event_split_size, MYF(MY_WME));
  ulonglong start_pos = 0,
            end_pos = opt_consensus_large_event_split_size;  // first batch
  while (start_pos < total_event_len) {
    ulonglong batch_size = end_pos - start_pos;

    assert(batch_size <= opt_consensus_large_event_split_size);

    if (end_pos == total_event_len && have_checksum) {
      /* if checksum enabled, write checksum for last batch */
      assert(start_pos < event_len &&
             total_event_len == event_len + BINLOG_CHECKSUM_LEN);
      memcpy(buffer, ev->temp_buf + start_pos,
             batch_size - BINLOG_CHECKSUM_LEN);
      memcpy(buffer, &ev_crc, BINLOG_CHECKSUM_LEN);
    } else {
      memcpy(buffer, ev->temp_buf + start_pos, batch_size);
    }

    total_batch_size += batch_size;

    // set flag
    if (total_batch_size == total_size)
      flag = Consensus_log_event_flag::FLAG_LARGE_TRX_END;
    else
      flag = Consensus_log_event_flag::FLAG_LARGE_TRX;

    if (end_pos == total_event_len) {
      flag |= Consensus_log_event_flag::FLAG_BLOB_END;
    } else if (start_pos == 0) {
      consensus_log_manager.get_fifo_cache_manager()->set_lock_blob_index(
          consensus_log_manager.get_current_index());
      flag |= (Consensus_log_event_flag::FLAG_BLOB |
               Consensus_log_event_flag::FLAG_BLOB_START);
    } else {
      flag |= Consensus_log_event_flag::FLAG_BLOB;
    }

    if (consensus_log_manager.get_first_event_in_file()) {
      flag |= Consensus_log_event_flag::FLAG_ROTATE;
      consensus_log_manager.set_first_event_in_file(false);
    }

    thd->consensus_context.consensus_index =
        consensus_log_manager.get_current_index();
    sql_print_information(
        "large_event_flush: event batch_size %llu, total_batch_size %llu, "
        "original buf_size "
        "%llu, consensus_index %llu.",
        batch_size, total_batch_size, total_size,
        thd->consensus_context.consensus_index);

    uint32 batch_crc =
        opt_consensus_checksum ? checksum_crc32(0, buffer, batch_size) : 0;
    if (end_pos != total_event_len) {
      std::string empty_log = consensus_log_manager.get_empty_log();
      Consensuslog_event_writer empty_log_writer(
          (Basic_ostream *)binlog_file,
          binlog_checksum_options != binary_log::BINLOG_CHECKSUM_ALG_OFF,
          binlog_file_get_current_pos(binlog_file) +
              Consensus_log_event::get_event_length() + ev_footer_size,
          true, true);

      sql_print_information(
          "large_event_flush: write empty log, the current pos(%lu), "
          "empty log size(%lu)",
          binlog_file_get_current_pos(binlog_file), empty_log.length());

      if (write_consensus_log_event(binlog_file, flag,
                                    thd->consensus_context.consensus_term,
                                    empty_log.length(), batch_crc)) {
        error = 1;
      } else {
        int4store((uchar *)empty_log.c_str(),
                  static_cast<long>(my_micro_time() / 1000000));
        if (empty_log_writer.write((const uchar *)empty_log.c_str(),
                                   empty_log.length())) {
          error = 1;
        }
      }

      flushed_size += empty_log.length() +
                      Consensus_log_event::get_event_length() + ev_footer_size;
    } else {
      sql_print_information(
          "large_event_flush: write payload log, the current pos(%lu), "
          "payload start pos(%lu), payload size(%lu)",
          binlog_file_get_current_pos(binlog_file), event_start_pos,
          total_event_len);

      /* Write consensus log entry with revised event to binlog file */
      if (write_consensus_log_event(binlog_file, flag,
                                    thd->consensus_context.consensus_term,
                                    total_event_len, batch_crc)) {
        error = 1;
      } else {
        error = write_buffer_to_binlog_file(
            binlog_file, (const uchar *)ev->temp_buf, event_len);
      }

      if (!error && have_checksum) {
        write_buffer_to_binlog_file(binlog_file, (const uchar *)&ev_crc,
                                    BINLOG_CHECKSUM_LEN);
      }

      flushed_size += total_event_len +
                      Consensus_log_event::get_event_length() + ev_footer_size;
    }
    if (!error) {
      consensus_log_manager.get_fifo_cache_manager()->add_log_to_cache(
          thd->consensus_context.consensus_term,
          thd->consensus_context.consensus_index, batch_size, buffer, false,
          flag, batch_crc);

      if (end_pos == total_event_len) {
        consensus_log_manager.get_fifo_cache_manager()->set_lock_blob_index(0);
      }

      if (!error) {
        error = binlog_file_flush_and_sync(binlog_file);
        consensus_log_manager.set_sync_index_if_greater(
            thd->consensus_context.consensus_index);
        rpl_consensus_write_log_done_internal(
            thd->consensus_context.consensus_index, true);
      }
    }

    DBUG_EXECUTE_IF("crash_during_large_event_binlog_flush", {
      static int ccnt = 0;
      ccnt++;
      if (ccnt > 1) {
        DBUG_SUICIDE();
      }
    });
    DBUG_EXECUTE_IF("crash_during_large_event_binlog_flush_slow",
                    { /* let follower get the log */
                      static int ccnt = 0;
                      ccnt++;
                      if (ccnt > 1) {
                        sleep(2);
                        DBUG_SUICIDE();
                      }
                    });

    // Advance start_pos and end_pos for next consensus log entry
    start_pos = end_pos;
    end_pos = end_pos + opt_consensus_large_event_split_size > total_event_len
                  ? total_event_len
                  : end_pos + opt_consensus_large_event_split_size;
  }
  return error;
}

static int large_trx_flush_log_cache(THD *thd,
                                     IO_CACHE_binlog_cache_storage *log_cache,
                                     MYSQL_BIN_LOG::Binlog_ofile *binlog_file,
                                     uint flag) {
  uint32 crc32 = 0;
  uchar *batch_content = nullptr;
  DBUG_TRACE;
  ulonglong batch_size = log_cache->length();

  if (consensus_log_manager.get_first_event_in_file()) {
    flag |= Consensus_log_event_flag::FLAG_ROTATE;
    consensus_log_manager.set_first_event_in_file(false);
  }

  sql_print_information(
      "large_trx_flush_log_cache: the current pos(%lu), "
      "payload size(%lu)",
      binlog_file_get_current_pos(binlog_file), batch_size);

  if ((opt_consensus_checksum && calc_consensus_crc(log_cache, crc32)) ||
      write_consensus_log_event(binlog_file, flag,
                                thd->consensus_context.consensus_term,
                                batch_size, crc32) ||
      stream_copy(log_cache, (Basic_ostream *)binlog_file)) {
    return 1;
  }

  batch_content = (uchar *)my_malloc(key_memory_thd_main_mem_root,
                                     (size_t)batch_size, MYF(MY_WME));
  DBUG_EXECUTE_IF("crash_during_large_trx_binlog_flush2", {
    static int ccnt = 0;
    ccnt++;
    if (ccnt > 1) {
      // force miss 1 byte and then crash
      copy_from_consensus_log_cache(log_cache, batch_content, batch_size-1);
      binlog_file_flush_and_sync(binlog_file);
      DBUG_SUICIDE();
    }
  });
  copy_from_consensus_log_cache(log_cache, batch_content, batch_size);

  if (consensus_log_manager.get_fifo_cache_manager()->add_log_to_cache(
          thd->consensus_context.consensus_term,
          thd->consensus_context.consensus_index, batch_size, batch_content,
          false, flag, crc32, true) == 1) {
    my_free(batch_content);
  }

  return 0;
}

static int large_trx_flush(THD *thd, ulonglong total_size, bool have_checksum,
                           Gtid_log_event &gtid_event,
                           binlog_cache_data *cache_data,
                           MYSQL_BIN_LOG::Binlog_ofile *binlog_file) {
  int error = 0;
  Log_event *ev = NULL;
  uint flag = 0;
  bool fisrt_event_in_batch = false;
  ulonglong batch_size = 0, total_batch_size = 0;
  ulonglong ev_footer_size = have_checksum ? BINLOG_CHECKSUM_LEN : 0;
  DBUG_TRACE;

  /* Init binlog cache data reader */
  Format_description_log_event fd_ev;
  Binlog_read_error m_error;
  Binlog_cache_storage *cache_storage = binlog_cache_get_storage(cache_data);
  IO_CACHE *payload_io_cache = cache_storage->get_io_cache();
  IO_cache_istream payload_istream(payload_io_cache);
  Binlog_event_data_istream payload_data_istream(&m_error, &payload_istream,
                                                 UINT_MAX);
  Binlog_event_object_istream<Binlog_event_data_istream> payload_event_istream(
      &m_error, &payload_data_istream);
  Default_binlog_event_allocator default_alloc;
  if (payload_istream.open()) return 1;

  /* Init consensus log writer */
  IO_CACHE_binlog_cache_storage *log_cache =
      consensus_log_manager.get_log_cache();
  Consensuslog_event_writer consensus_writer(
      log_cache, have_checksum,
      binlog_file_get_current_pos(binlog_file) +
          /* consensus log event for first batch */
          Consensus_log_event::get_event_length() + ev_footer_size);
  log_cache->reset();

  /* Write gtid log event */
  gtid_event.write(&consensus_writer);

  while (!error && (ev = payload_event_istream.read_event_object(
                        fd_ev, false, &default_alloc)) != NULL) {
    ulonglong event_len = uint4korr(ev->temp_buf + EVENT_LEN_OFFSET);
    ulonglong event_total_len = event_len + ev_footer_size;
    batch_size = log_cache->length();

    assert(ev->common_header->type_code !=
           binary_log::FORMAT_DESCRIPTION_EVENT);

    if (batch_size > 0 &&
        (batch_size + event_total_len >
             opt_consensus_max_log_size || /* overflow with current event */
         DBUG_EVALUATE_IF("force_large_trx_single_ev", true, false))) {
      flag = Consensus_log_event_flag::FLAG_LARGE_TRX;
      thd->consensus_context.consensus_index =
          consensus_log_manager.get_current_index();

      total_batch_size += batch_size;

      sql_print_information(
          "Large trx batch_size %llu, total_batch_size %llu, original buf_size "
          "%llu, consensus_index %llu.",
          batch_size, total_batch_size, total_size,
          thd->consensus_context.consensus_index);

      /* Write consensus log entry with current batch to binlog file */
      large_trx_flush_log_cache(thd, log_cache, binlog_file, flag);

      // Reset log payload cache
      fisrt_event_in_batch = true;
      log_cache->reset();
      batch_size = 0;

      /* for large trx, sync directly after flush for performance */
      if (!error) {
        error = binlog_file_flush_and_sync(binlog_file);
        consensus_log_manager.set_sync_index_if_greater(
            thd->consensus_context.consensus_index);
        rpl_consensus_write_log_done_internal(
            thd->consensus_context.consensus_index, true);
      }

      DEBUG_SYNC(thd, "large_trx_sync_part");
      DBUG_EXECUTE_IF("crash_during_large_trx_binlog_flush", {
        static int ccnt = 0;
        ccnt++;
        if (ccnt > 1) {
          DBUG_SUICIDE();
        }
      });
      DBUG_EXECUTE_IF("crash_during_large_trx_binlog_flush_slow",
                      { /* let follower get the log */
                        static int ccnt = 0;
                        ccnt++;
                        if (ccnt > 1) {
                          sleep(2);
                          DBUG_SUICIDE();
                        }
                      });
    }

    if (event_total_len > opt_consensus_max_log_size) {
      ulonglong flush_size = 0;
      /* current ev is large event */
      assert(batch_size == 0 && fisrt_event_in_batch);
      error = large_event_flush(thd, total_size, ev, event_len, event_total_len,
                                have_checksum, binlog_file, total_batch_size,
                                flush_size);

      consensus_writer.inc_end_log_pos(flush_size);
    } else {
      if (fisrt_event_in_batch) {
        consensus_writer.inc_end_log_pos(
            Consensus_log_event::get_event_length() + ev_footer_size);
        fisrt_event_in_batch = false;
      }
      /* Write the event to consensus log cache */
      consensus_writer.write((uchar *)ev->temp_buf, event_len);
    }
    delete ev;
  }
  /* deal with remained buffer */
  if (log_cache->length() > 0) {
    flag = Consensus_log_event_flag::FLAG_LARGE_TRX_END;
    thd->consensus_context.consensus_index =
        consensus_log_manager.get_current_index();
    sql_print_information(
        "Large trx batch_size %llu, total_batch_size %llu, original buf_size "
        "%llu, consensus_index %llu.",
        log_cache->length(), total_batch_size, total_size,
        thd->consensus_context.consensus_index);
    large_trx_flush_log_cache(thd, log_cache, binlog_file, flag);
    log_cache->reset();
  }
  return error;
}

void consensus_before_commit(THD *thd) {
  DBUG_TRACE;
  if ((rpl_consensus_wait_commit_index_update(
           thd->consensus_context.consensus_index,
           thd->consensus_context.consensus_term) <
       thd->consensus_context.consensus_index)) {
    if (rpl_consensus_is_shutdown())
      thd->consensus_context.consensus_error =
          Consensus_binlog_context_info::CSS_SHUTDOWN;

    if (thd->consensus_context.consensus_error ==
        Consensus_binlog_context_info::CSS_NONE) {
      // Wait for the state degrade term to be updated
      while (thd->consensus_context.consensus_term >=
             consensus_log_manager.get_current_state_degrade_term())
        my_sleep(1000);

      // Rollback the transaction if the index is more than
      // start_apply_index
      if (thd->consensus_context.consensus_index >
          consensus_log_manager.get_consensus_info()->get_start_apply_index())
        thd->consensus_context.consensus_error =
            Consensus_binlog_context_info::CSS_LEADERSHIP_CHANGE;
    }
  }
}

void consensus_update_pos_map_by_file_name(std::string &filename,
                                           Consensus_log_event *ev,
                                           uint64 start_pos, uint64 next_pos,
                                           bool &next_set) {
  DBUG_TRACE;
  /* Normal consensus entry or first part of large event */
  if (!next_set &&
      !(ev->get_flag() & Consensus_log_event_flag::FLAG_BLOB_END) &&
      (!(ev->get_flag() & Consensus_log_event_flag::FLAG_BLOB) ||
       (ev->get_flag() & Consensus_log_event_flag::FLAG_BLOB_START))) {
    consensus_log_manager.get_log_file_index()->update_pos_map_by_file_name(
        filename, ev->get_index(), start_pos);
  }

  /* Not large event */
  if (!(ev->get_flag() & (Consensus_log_event_flag::FLAG_BLOB |
                          Consensus_log_event_flag::FLAG_BLOB_START |
                          Consensus_log_event_flag::FLAG_BLOB_END))) {
    consensus_log_manager.get_log_file_index()->update_pos_map_by_file_name(
        filename, ev->get_index() + 1, next_pos);
    next_set = true;
  } else {
    next_set = false;
  }
}

void update_pos_map_by_start_index(const uint64 start_index,
                                   Consensus_log_event *ev, uint64 start_pos,
                                   uint64 end_pos, bool &next_set) {
  DBUG_TRACE;
  /* Normal consensus entry or first part of large event. And not set by
   * previous consensus event */
  if (!next_set &&
      !(ev->get_flag() & Consensus_log_event_flag::FLAG_BLOB_END) &&
      (!(ev->get_flag() & Consensus_log_event_flag::FLAG_BLOB) ||
       (ev->get_flag() & Consensus_log_event_flag::FLAG_BLOB_START))) {
    consensus_log_manager.get_log_file_index()->update_pos_map_by_start_index(
        start_index, ev->get_index(), start_pos);
  }

  /* Not large event. Set next index pos by end position */
  if (!(ev->get_flag() & (Consensus_log_event_flag::FLAG_BLOB |
                          Consensus_log_event_flag::FLAG_BLOB_START |
                          Consensus_log_event_flag::FLAG_BLOB_END))) {
    consensus_log_manager.get_log_file_index()->update_pos_map_by_start_index(
        start_index, ev->get_index() + 1, end_pos);
    next_set = true;
  } else {
    next_set = false;
  }
}

static int get_lower_bound_pos_of_index(const uint64 start_index,
                                        const uint64 consensus_index,
                                        uint64 &pos, bool &matched) {
  DBUG_TRACE;
  int res =
      consensus_log_manager.get_log_file_index()->get_lower_bound_pos_of_index(
          start_index, consensus_index, pos, matched);

  sql_print_information(
      "Consensus get low bound pos of index %llu from index %llu. pos %llu "
      "matched %d",
      consensus_index, start_index, pos, matched);

  return res;
}

int consensus_find_log_by_index(uint64 consensus_index, std::string &file_name,
                                uint64 &start_index) {
  return consensus_log_manager.get_log_file_index()->get_log_file_from_index(
      consensus_index, file_name, start_index);
}

int consensus_get_first_index() {
  return consensus_log_manager.get_log_file_index()->get_first_index();
}

static int fetch_binlog_by_offset(Binlog_file_reader &binlog_file_reader,
                                  uint64 start_pos, uint64 end_pos,
                                  Consensus_cluster_info_log_event *rci_ev,
                                  std::string &log_content) {
  DBUG_TRACE;

  if (start_pos == end_pos) {
    log_content.assign("");
    return 0;
  }
  if (rci_ev == NULL) {
    unsigned int buf_size = end_pos - start_pos;
    uchar *buffer =
        (uchar *)my_malloc(key_memory_thd_main_mem_root, buf_size, MYF(MY_WME));
    binlog_file_reader.seek(start_pos);
    binlog_file_reader.ifile()->read(buffer, buf_size);
    log_content.assign((char *)buffer, buf_size);
    my_free(buffer);
  } else {
    log_content.assign(rci_ev->get_info(), (size_t)rci_ev->get_info_length());
  }
  return 0;
}

static int prefetch_logs_of_file(THD *thd, uint64 channel_id,
                                 const char *file_name, uint64 file_start_index,
                                 uint64 start_index) {
  DBUG_TRACE;

  bool next_set = false;
  uint64 lower_start_pos;
  bool matched;
  get_lower_bound_pos_of_index(file_start_index, start_index, lower_start_pos,
                               matched);
  if (lower_start_pos == 0) lower_start_pos = BIN_LOG_HEADER_SIZE;

  Binlog_file_reader binlog_file_reader(opt_source_verify_checksum);
  if (binlog_file_reader.open(file_name, lower_start_pos)) {
    LogErr(ERROR_LEVEL, ER_BINLOG_FILE_OPEN_FAILED,
           binlog_file_reader.get_error_str());
    return 1;
  }

  Log_event *ev = NULL;
  Consensus_cluster_info_log_event *rci_ev = NULL;
  Consensus_log_event *consensus_log_ev = NULL;
  uint64 start_pos = binlog_file_reader.position();
  uint64 end_pos = start_pos;

  uint64 current_index = 0;
  uint64 current_term = 0;
  uint32 consensus_log_length = 0;
  uint current_flag = 0;
  uint64 current_crc32 = 0;
  bool stop_prefetch = false;
  std::string log_content;
  std::vector<uint64> blob_index_list;
  std::vector<uint64> blob_term_list;
  std::vector<uint64> blob_flag_list;
  std::vector<uint64> blob_crc32_list;
  ConsensusPreFetchManager *prefetch_mgr =
      consensus_log_manager.get_prefetch_manager();
  ConsensusPreFetchChannel *prefetch_channel =
      prefetch_mgr->get_prefetch_channel(channel_id);
  prefetch_channel->set_prefetching(true);
  if (prefetch_channel->get_channel_id() == 0)
    prefetch_channel->clear_large_trx_table();
  while (!stop_prefetch &&
         (ev = binlog_file_reader.read_event_object()) != NULL) {
    switch (ev->get_type_code()) {
      case binary_log::CONSENSUS_LOG_EVENT:
        consensus_log_ev = (Consensus_log_event *)ev;
        current_index = consensus_log_ev->get_index();
        current_term = consensus_log_ev->get_term();
        consensus_log_length = consensus_log_ev->get_length();
        current_flag = consensus_log_ev->get_flag();
        current_crc32 = consensus_log_ev->get_reserve();
        end_pos = start_pos = binlog_file_reader.position();

        update_pos_map_by_start_index(file_start_index, consensus_log_ev,
                                      binlog_file_reader.event_start_pos(),
                                      start_pos + consensus_log_length,
                                      next_set);

        if (opt_consensus_prefetch_fast_fetch) {
          /*
           * jump to next consensus_log_event:
           * 1. not large trx, not blob
           * 2. current_index + window_size < start_index
           */
          if ((!(current_flag & (Consensus_log_event_flag::FLAG_LARGE_TRX |
                                 Consensus_log_event_flag::FLAG_LARGE_TRX_END |
                                 Consensus_log_event_flag::FLAG_BLOB |
                                 Consensus_log_event_flag::FLAG_BLOB_START |
                                 Consensus_log_event_flag::FLAG_BLOB_END))) &&
              (current_index + prefetch_channel->get_window_size() <
               start_index)) {
            binlog_file_reader.seek(start_pos + consensus_log_length);
          }
          /*
           * fetch data directly:
           * 1. not large trx, not blob
           * 2. not configure change
           * 3. current_index + window_size >= start_index
           */
          if ((!(current_flag &
                 (Consensus_log_event_flag::FLAG_LARGE_TRX |
                  Consensus_log_event_flag::FLAG_LARGE_TRX_END |
                  Consensus_log_event_flag::FLAG_BLOB |
                  Consensus_log_event_flag::FLAG_BLOB_START |
                  Consensus_log_event_flag::FLAG_BLOB_END |
                  Consensus_log_event_flag::FLAG_CONFIG_CHANGE))) &&
              (current_index + prefetch_channel->get_window_size() >=
               start_index)) {
            uchar *buffer =
                (uchar *)my_malloc(key_memory_thd_main_mem_root,
                                   consensus_log_length, MYF(MY_WME));
            binlog_file_reader.ifile()->read(buffer, consensus_log_length);
            int result = 0;
            while ((result = prefetch_channel->add_log_to_prefetch_cache(
                        current_term, current_index, consensus_log_length,
                        buffer, false, current_flag, current_crc32)) == FULL) {
              // wait condition already executed in add log to prefetch cache
            }
            if (result == INTERRUPT ||
                current_index == consensus_log_manager.get_sync_index())
              stop_prefetch = true;
            my_free(buffer);
            end_pos = binlog_file_reader.position();
            assert(end_pos - start_pos == consensus_log_length);
          }
        }
        break;
      default:
        if (!ev->is_control_event()) {
          end_pos = binlog_file_reader.position();
          if (ev->get_type_code() == binary_log::CONSENSUS_CLUSTER_INFO_EVENT) {
            rci_ev = static_cast<Consensus_cluster_info_log_event *>(ev);
          }
          if (end_pos > start_pos &&
              end_pos - start_pos == consensus_log_length) {
            if (prefetch_channel->get_channel_id() == 0 &&
                (current_flag &
                 (Consensus_log_event_flag::FLAG_LARGE_TRX |
                  Consensus_log_event_flag::FLAG_LARGE_TRX_END))) {
              prefetch_channel->add_log_to_large_trx_table(
                  current_term, current_index, (rci_ev != NULL), current_flag);
            }
            if (current_flag & Consensus_log_event_flag::FLAG_BLOB) {
              blob_index_list.push_back(current_index);
              blob_term_list.push_back(current_term);
              blob_flag_list.push_back(current_flag);
              blob_crc32_list.push_back(current_crc32);
            } else if (current_flag & Consensus_log_event_flag::FLAG_BLOB_END) {
              blob_index_list.push_back(current_index);
              blob_term_list.push_back(current_term);
              blob_flag_list.push_back(current_flag);
              blob_crc32_list.push_back(current_crc32);
              uint64 split_len = opt_consensus_large_event_split_size;
              uint64 blob_start_pos = start_pos,
                     blob_end_pos = start_pos + split_len;
              for (size_t i = 0; i < blob_index_list.size(); ++i) {
                if (blob_index_list[i] + prefetch_channel->get_window_size() >=
                    start_index) {
                  fetch_binlog_by_offset(binlog_file_reader, blob_start_pos,
                                         blob_end_pos, NULL, log_content);
                  int result = 0;
                  while ((result = prefetch_channel->add_log_to_prefetch_cache(
                              blob_term_list[i], blob_index_list[i],
                              log_content.size(),
                              reinterpret_cast<uchar *>(
                                  const_cast<char *>(log_content.c_str())),
                              false, blob_flag_list[i], blob_crc32_list[i])) ==
                         FULL) {
                    // wait condition already executed in add log to prefetch
                    // cache
                  }
                  if (result == INTERRUPT) {
                    stop_prefetch = true;
                    break;  // break iterate blob_index_list
                  }
                }
                blob_start_pos = blob_end_pos;
                blob_end_pos = blob_end_pos + split_len > end_pos
                                   ? end_pos
                                   : blob_end_pos + split_len;
              }
              blob_index_list.clear();
              blob_term_list.clear();
              blob_flag_list.clear();
              blob_crc32_list.clear();
            } else {
              if (current_index + prefetch_channel->get_window_size() >=
                  start_index) {
                fetch_binlog_by_offset(binlog_file_reader, start_pos, end_pos,
                                       rci_ev, log_content);
                int result = 0;
                while ((result = prefetch_channel->add_log_to_prefetch_cache(
                            current_term, current_index, log_content.size(),
                            reinterpret_cast<uchar *>(
                                const_cast<char *>(log_content.c_str())),
                            (rci_ev != NULL), current_flag, current_crc32)) ==
                       FULL) {
                  // wait condition already executed in add log to prefetch
                  // cache
                }
                if (result == INTERRUPT ||
                    current_index == consensus_log_manager.get_sync_index()) {
                  stop_prefetch = true;  // because truncate log happened, stop
                                         // prefetch and retry
                }
              }
            }
            rci_ev = NULL;
          }
        }
        break;
    }
    delete ev;
  }

  prefetch_channel->set_prefetching(false);
  prefetch_channel->dec_ref_count();
  prefetch_channel->clear_prefetch_request();

  sql_print_information(
      "channel_id %llu prefetch log reached consensus index %llu", channel_id,
      (uint64)current_index);

  return 0;
}

static int read_log_by_consensus_index(
    const char *file_name, uint64 start_index, uint64 consensus_index,
    uint64 *consensus_term, std::string &log_content, bool *outer, uint *flag,
    uint64 *checksum, bool need_content) {
  uint64 lower_start_pos;
  bool next_set = false;
  bool matched;
  DBUG_TRACE;

  get_lower_bound_pos_of_index(start_index, consensus_index, lower_start_pos,
                               matched);
  if (lower_start_pos == 0) lower_start_pos = BIN_LOG_HEADER_SIZE;

  Binlog_file_reader binlog_file_reader(opt_source_verify_checksum);
  if (binlog_file_reader.open(file_name, lower_start_pos)) {
    LogErr(ERROR_LEVEL, ER_BINLOG_FILE_OPEN_FAILED,
           binlog_file_reader.get_error_str());
    return 1;
  }

  Log_event *ev = NULL;
  Consensus_cluster_info_log_event *rci_ev = NULL;
  Consensus_log_event *consensus_log_ev = NULL;
  bool found = false;
  bool stop_scan = false;
  bool in_transaction = false;
  uint64 start_pos = lower_start_pos;
  uint64 end_pos = start_pos;
  uint64 consensus_log_length = 0;
  uint64 cindex, cterm, cflag, ccrc32;
  std::vector<uint64> blob_index_list;
  std::vector<uint64> blob_term_list;
  std::vector<uint64> blob_flag_list;
  std::vector<uint64> blob_crc32_list;
  while (!stop_scan && (ev = binlog_file_reader.read_event_object()) != NULL) {
    switch (ev->get_type_code()) {
      case binary_log::CONSENSUS_LOG_EVENT:
        consensus_log_ev = (Consensus_log_event *)ev;
        cindex = consensus_log_ev->get_index();
        cterm = consensus_log_ev->get_term();
        cflag = consensus_log_ev->get_flag();
        ccrc32 = consensus_log_ev->get_reserve();
        consensus_log_length = consensus_log_ev->get_length();
        end_pos = start_pos = binlog_file_reader.position();
        if (consensus_index == cindex) {
          found = true;
          *consensus_term = cterm;
          *flag = cflag;
          *checksum = ccrc32;
        } else if (!found && consensus_log_ev->get_index() > consensus_index) {
          sql_print_information("directly read log error, log size is error");
          abort();
        }
        update_pos_map_by_start_index(
            start_index, consensus_log_ev, binlog_file_reader.event_start_pos(),
            binlog_file_reader.position() + consensus_log_ev->get_length(),
            next_set);
        break;
      default:
        if (!ev->is_control_event()) {
          end_pos = binlog_file_reader.position();
          if (ev->get_type_code() == binary_log::CONSENSUS_CLUSTER_INFO_EVENT &&
              found) {
            rci_ev = (Consensus_cluster_info_log_event *)ev;
          }
          if (end_pos > start_pos &&
              end_pos - start_pos == consensus_log_length) {
            if (need_content && (cflag & Consensus_log_event_flag::FLAG_BLOB)) {
              blob_index_list.push_back(cindex);
              blob_term_list.push_back(cterm);
              blob_flag_list.push_back(cflag);
              blob_crc32_list.push_back(ccrc32);
            } else if (need_content &&
                       (cflag & Consensus_log_event_flag::FLAG_BLOB_END)) {
              blob_index_list.push_back(cindex);
              blob_term_list.push_back(cterm);
              blob_flag_list.push_back(cflag);
              blob_crc32_list.push_back(ccrc32);
              if (found) {
                assert(consensus_index >= blob_index_list[0] &&
                       consensus_index <= cindex);
                /* It means the required index is between a blob event */
                uint64 split_len = opt_consensus_large_event_split_size;
                uint64 blob_start_pos = start_pos,
                       blob_end_pos = start_pos + split_len;
                for (size_t i = 0; i < blob_index_list.size(); ++i) {
                  if (blob_index_list[i] == consensus_index) {
                    fetch_binlog_by_offset(binlog_file_reader, blob_start_pos,
                                           blob_end_pos, NULL, log_content);
                    *outer = false;
                    end_pos = start_pos = binlog_file_reader.position();
                    stop_scan = true;
                    break;
                  }
                  blob_start_pos = blob_end_pos;
                  blob_end_pos = blob_end_pos + split_len > end_pos
                                     ? end_pos
                                     : blob_end_pos + split_len;
                }
              }
              blob_index_list.clear();
              blob_term_list.clear();
              blob_flag_list.clear();
              blob_crc32_list.clear();
            } else {
              if (found) {
                if (need_content || rci_ev != NULL)
                  fetch_binlog_by_offset(binlog_file_reader, start_pos, end_pos,
                                         rci_ev, log_content);
                *outer = (rci_ev != NULL);
                end_pos = start_pos = binlog_file_reader.position();
                stop_scan = true;
                rci_ev = NULL;
              }
            }
          }
        }
        break;
    }
    delete ev;
  }

  // if scan to end of file
  if (end_pos > start_pos && !in_transaction) {
    if (need_content || rci_ev != NULL)
      fetch_binlog_by_offset(binlog_file_reader, start_pos, end_pos, rci_ev,
                             log_content);
    sql_print_information("Consensus read last log size %llu directly",
                          end_pos - start_pos);
    end_pos = start_pos = binlog_file_reader.position();
  }

  sql_print_information("Consensus read log reached consensus index %llu directly",
                        consensus_index);

  if (!found) sql_print_error("Consensus read log by consensus index failed");

  // mysql_file_close(file, MYF(MY_WME));
  // end_io_cache(&log);

  return (int)!found;
}

int consensus_get_log_entry(uint64 consensus_index, uint64 *consensus_term,
                            std::string &log_content, bool *outer, uint *flag,
                            uint64 *checksum, bool need_content) {
  std::string file_name;
  uint64 start_index;
  int ret = 0;
  DBUG_TRACE;

  if (consensus_find_log_by_index(consensus_index, file_name, start_index) ||
      read_log_by_consensus_index(file_name.c_str(), start_index,
                                  consensus_index, consensus_term, log_content,
                                  outer, flag, checksum, need_content)) {
    sql_print_error("Get log entry cannot find consensus index log %llu.",
                    consensus_index);
    ret = 1;
  }

  return ret;
}

int consensus_prefetch_log_entries(THD *thd, uint64 channel_id,
                                   uint64 consensus_index) {
  std::string file_name;
  uint64 start_index;
  int ret = 0;
  DBUG_TRACE;

  // use another io_cache , so do not need lock LOCK_log
  if (consensus_find_log_by_index(consensus_index, file_name, start_index) ||
      prefetch_logs_of_file(thd, channel_id, file_name.c_str(), start_index,
                            consensus_index)) {
    sql_print_error("Prefetch cannot find consensus index log %llu.",
                    consensus_index);
    ret = 1;
  }

  return ret;
}

static void store_gtid_for_consensus_log(const char *buf, Relay_log_info *rli) {
  Log_event_type event_type = (Log_event_type)buf[EVENT_TYPE_OFFSET];
  DBUG_TRACE;

  if (event_type == binary_log::GTID_LOG_EVENT) {
    Format_description_log_event fd_ev;
    fd_ev.footer()->checksum_alg =
        static_cast<enum_binlog_checksum_alg>(binlog_checksum_options);
    Gtid_log_event gtid_ev(buf, &fd_ev);
    rli->get_sid_lock()->wrlock();
    rli->add_logged_gtid(rli->get_sid_map()->add_sid(*gtid_ev.get_sid()),
                         gtid_ev.get_gno());
    rli->get_sid_lock()->unlock();
  }
}

uint64 consensus_get_trx_end_index(uint64 firstIndex) {
  std::string file_name;
  uint64 start_index;
  uint64 start_pos;
  bool matched;
  Log_event *ev = NULL;
  Consensus_log_event *consensus_log_ev = NULL;
  bool stop_scan = false;
  uint64 currentIndex = 0;
  uint64 currentFlag = 0;
  DBUG_TRACE;

  // use another io_cache , so do not need lock LOCK_log
  if (consensus_find_log_by_index(firstIndex, file_name, start_index)) {
    sql_print_error(
        "consensus_get_trx_end_index cannot find consensus index log %llu.",
        firstIndex);
    return 0;
  }

  /* Search lower bound of pos from cached pos map */
  (void)get_lower_bound_pos_of_index(start_index, firstIndex, start_pos,
                                     matched);
  if (start_pos == 0) start_pos = BIN_LOG_HEADER_SIZE;

  Binlog_file_reader binlog_file_reader(opt_source_verify_checksum);
  if (binlog_file_reader.open(file_name.c_str(), start_pos)) {
    LogErr(ERROR_LEVEL, ER_BINLOG_FILE_OPEN_FAILED,
           binlog_file_reader.get_error_str());
    return 0;
  }

  while (!stop_scan && (ev = binlog_file_reader.read_event_object()) != NULL) {
    switch (ev->get_type_code()) {
      case binary_log::CONSENSUS_LOG_EVENT:
        consensus_log_ev = (Consensus_log_event *)ev;
        currentIndex = consensus_log_ev->get_index();
        currentFlag = consensus_log_ev->get_flag();
        if (firstIndex <= currentIndex &&
            !(currentFlag & Consensus_log_event_flag::FLAG_LARGE_TRX))
          stop_scan = true;
        break;
      default:
        break;
    }
    delete ev;
  }

  return stop_scan ? currentIndex : 0;
}

/*
   There are 3 condition to determine the right position
   1. beginning of the index
   2. ending of the previous index
   3. beginning of the binlog file
*/
int consensus_find_pos_by_index(const char *file_name, const uint64 start_index,
                                const uint64 consensus_index, uint64 *pos) {
  uint64 start_pos;
  bool next_set = false;
  bool matched;
  DBUG_TRACE;

  get_lower_bound_pos_of_index(start_index, consensus_index, start_pos,
                               matched);
  if (matched) {
    *pos = start_pos;
    return 0;
  }

  if (start_pos == 0) start_pos = BIN_LOG_HEADER_SIZE;

  Binlog_file_reader binlog_file_reader(opt_source_verify_checksum);
  if (binlog_file_reader.open(file_name, start_pos)) {
    LogErr(ERROR_LEVEL, ER_BINLOG_FILE_OPEN_FAILED,
           binlog_file_reader.get_error_str());
    return 1;
  }

  Log_event *ev = NULL;
  Consensus_log_event *consensus_log_ev = NULL;
  Previous_consensus_index_log_event *consensus_prev_ev = NULL;
  bool found = false;
  bool first_log_in_file = false;

  while (!found && (ev = binlog_file_reader.read_event_object()) != NULL) {
    switch (ev->get_type_code()) {
      case binary_log::CONSENSUS_LOG_EVENT:
        consensus_log_ev = (Consensus_log_event *)ev;
        if (consensus_index == consensus_log_ev->get_index()) found = true;
        if (consensus_index == consensus_log_ev->get_index() + 1) {
          found = true;
          *pos = binlog_file_reader.position() + consensus_log_ev->get_length();
        }
        update_pos_map_by_start_index(
            start_index, consensus_log_ev, binlog_file_reader.event_start_pos(),
            binlog_file_reader.position() + consensus_log_ev->get_length(),
            next_set);
        break;
      case binary_log::PREVIOUS_CONSENSUS_INDEX_LOG_EVENT:
        consensus_prev_ev = (Previous_consensus_index_log_event *)ev;
        if (consensus_index == consensus_prev_ev->get_index())
          first_log_in_file = true;
        break;
      case binary_log::PREVIOUS_GTIDS_LOG_EVENT:
        if (first_log_in_file) {
          *pos = binlog_file_reader.position();
          found = true;
        }
        break;
      default:
        break;
    }
    delete ev;
  }
  if (binlog_file_reader.has_fatal_error()) {
    LogErr(ERROR_LEVEL, ER_BINLOG_FILE_OPEN_FAILED,
           binlog_file_reader.get_error_str());
  }

  return !found;
}

int consensus_get_log_position(uint64 consensus_index, char *log_name,
                               uint64 *pos) {
  std::string file_name;
  uint64 start_index;
  int ret = 0;
  DBUG_TRACE;

  // use another io_cache , so do not need lock LOCK_log
  if (consensus_find_log_by_index(consensus_index, file_name, start_index) ||
      consensus_find_pos_by_index(file_name.c_str(), start_index,
                                  consensus_index, pos)) {
    sql_print_error("Get log position cannot find consensus index log %llu.",
                    consensus_index);
    ret = 1;
  }
  strncpy(log_name, file_name.c_str(), FN_REFLEN);
  return ret;
}

#if 0
/**
  @brief Checks if automatic purge size conditions are met and therefore the
  purge is allowed to be done. If not met returns true. Otherwise, false.

  @return false if the check is successful. True otherwise.
*/

static bool check_auto_purge_size_condition() {
  if (!is_consensus_replication_enabled()) return true;

  if (binlog_purge_size == 0) return true;

  // get the size of the binary logs
  ulonglong total_binlog_size =
      consensus_log_manager.get_log_file_index()->get_total_log_size();

  if (total_binlog_size < binlog_purge_size) return true;

  return false;
}
#endif

int consensus_binlog_after_purge_file(MYSQL_BIN_LOG *binlog,
                                      const char *log_file_name) {
  int error = 0;
  DBUG_TRACE;

  if (!binlog->is_relay_log) {
    std::string file_name = std::string(log_file_name);
    consensus_log_manager.get_log_file_index()->truncate_before(file_name);

    global_sid_lock->wrlock();
    error = binlog->consensus_init_gtid_sets(
        nullptr, const_cast<Gtid_set *>(gtid_state->get_lost_gtids()),
        opt_source_verify_checksum, false);
    global_sid_lock->unlock();
  }

  return error;
}

bool consensus_show_log_events(THD *thd) {
  bool ret = false;
  DBUG_TRACE;

  assert(thd->lex->sql_command == SQLCOM_SHOW_CONSENSUSLOG_EVENTS);

  mem_root_deque<Item *> field_list(thd->mem_root);
  Log_event::init_show_field_list(&field_list);
  if (thd->send_result_metadata(field_list,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    return true;

  /*
  Wait for handlers to insert any pending information
  into the binlog.  For e.g. ndb which updates the binlog asynchronously
  this is needed so that the uses sees all its own commands in the binlog
  */
  ha_binlog_wait(thd);

  mysql_rwlock_rdlock(consensus_log_manager.get_consensuslog_status_lock());
  MYSQL_BIN_LOG *log =
      consensus_log_manager.get_status() == BINLOG_WORKING
          ? &mysql_bin_log
          : &consensus_log_manager.get_relay_log_info()->relay_log;

  if (log->is_open()) {
    LEX_CONSENSUS_INFO *lex_ci = &thd->lex->consensus;
    LEX_MASTER_INFO *lex_mi = &thd->lex->mi;
    char search_file_name[FN_REFLEN];
    uint64 first_index = consensus_get_first_index();
    uint64 consensus_index =
        lex_ci->log_index < first_index ? first_index : lex_ci->log_index;
    uint64 pos = BIN_LOG_HEADER_SIZE;

    if (consensus_get_log_position(consensus_index, search_file_name, &pos)) {
      mysql_rwlock_unlock(consensus_log_manager.get_consensuslog_status_lock());
      my_error(ER_CONSENSUS_INDEX_NOT_VALID, MYF(0)); /* purecov: inspected */
      return true;
    }

    lex_mi->log_file_name = thd->mem_strdup(search_file_name);
    lex_mi->pos = pos;
  }

  ret = show_binlog_events(thd, log);

  mysql_rwlock_unlock(consensus_log_manager.get_consensuslog_status_lock());

  return ret;
}

bool consensus_show_logs(THD *thd) {
  IO_CACHE *index_file;
  LOG_INFO cur;
  File file;
  char fname[FN_REFLEN];
  mem_root_deque<Item *> field_list(thd->mem_root);
  size_t length;
  size_t cur_dir_len;
  Protocol *protocol = thd->get_protocol();
  DBUG_TRACE;

  field_list.push_back(new Item_empty_string("Log_name", 255));
  field_list.push_back(
      new Item_return_int("File_size", 20, MYSQL_TYPE_LONGLONG));
  field_list.push_back(
      new Item_return_int("Start_log_index", 20, MYSQL_TYPE_LONGLONG));

  if (thd->send_result_metadata(field_list,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    return true;

  mysql_rwlock_rdlock(consensus_log_manager.get_consensuslog_status_lock());
  MYSQL_BIN_LOG *log =
      consensus_log_manager.get_status() == BINLOG_WORKING
          ? &mysql_bin_log
          : &consensus_log_manager.get_relay_log_info()->relay_log;
  if (!log->is_open()) {
    mysql_rwlock_unlock(consensus_log_manager.get_consensuslog_status_lock());
    my_error(ER_NO_BINARY_LOGGING, MYF(0));
    return true;
  }

  mysql_mutex_lock(log->get_log_lock());

  DEBUG_SYNC(thd, "show_binlogs_after_lock_log_before_lock_index");
  log->lock_index();
  index_file = log->get_index_file();

  log->raw_get_current_log(&cur);           // dont take mutex
  mysql_mutex_unlock(log->get_log_lock());  // lockdep, OK
  mysql_rwlock_unlock(consensus_log_manager.get_consensuslog_status_lock());

  cur_dir_len = dirname_length(cur.log_file_name);

  reinit_io_cache(index_file, READ_CACHE, (my_off_t)0, 0, 0);

  /* The file ends with EOF or empty line */
  while ((length = my_b_gets(index_file, fname, sizeof(fname))) > 1) {
    size_t dir_len;
    int encrypted_header_size = 0;
    ulonglong file_length = 0;  // Length if open fails
    fname[--length] = '\0';     // remove the newline

    protocol->start_row();
    dir_len = dirname_length(fname);
    length -= dir_len;
    protocol->store_string(fname + dir_len, length, &my_charset_bin);

    if (!(strncmp(fname + dir_len, cur.log_file_name + cur_dir_len, length))) {
      /* Encryption header size shall be accounted in the file_length */
      encrypted_header_size = cur.encrypted_header_size;
      file_length = cur.pos; /* The active log, use the active position */
      file_length = file_length + encrypted_header_size;
    } else {
      /* this is an old log, open it and find the size */
      if ((file = mysql_file_open(key_file_binlog, fname, O_RDONLY, MYF(0))) >=
          0) {
        file_length = (ulonglong)mysql_file_seek(file, 0L, MY_SEEK_END, MYF(0));
        mysql_file_close(file, MYF(0));
      }
    }
    protocol->store(file_length);

    ulonglong start_index =
        consensus_log_manager.get_log_file_index()->get_start_index_of_file(
            std::string(fname));
    protocol->store(start_index);
    if (protocol->end_row()) {
      DBUG_PRINT(
          "info",
          ("stopping dump thread because protocol->write failed at line %d",
           __LINE__));
      goto err;
    }
  }
  if (index_file->error == -1) goto err;
  log->unlock_index();
  my_eof(thd);
  return false;

err:
  log->unlock_index();
  return true;
}

static uint32 abstract_event_timestamp_from_cache(
    IO_CACHE_binlog_cache_storage *log_cache) {
  uchar header[LOG_EVENT_HEADER_LEN];
  uchar *buffer = nullptr;
  my_off_t length = 0;
  size_t event_left_len = 0;
  size_t header_len = 0;
  uint32 tv_event = 0;
  DBUG_TRACE;

  bool ret = log_cache->begin(&buffer, &length);

  // Loop through the log cache
  while (!ret && length > 0) {
    if (event_left_len == 0) {
      size_t header_incr =
          std::min<size_t>(LOG_EVENT_HEADER_LEN - header_len, length);
      memcpy(header + header_len, buffer, header_incr);
      buffer += header_incr;
      header_len += header_incr;
      length -= header_incr;

      if (header_len == LOG_EVENT_HEADER_LEN) {
        tv_event = uint4korr(header);
        if (tv_event > 0) break;

        event_left_len = uint4korr(header + EVENT_LEN_OFFSET);
        event_left_len -= LOG_EVENT_HEADER_LEN;
        header_len = 0;
      }
    }

    if (event_left_len > 0) {
      size_t event_incr = std::min<size_t>(event_left_len, length);
      event_left_len -= event_incr;
      length -= event_incr;
      buffer += event_incr;
    }

    if (length == 0) ret = log_cache->next(&buffer, &length); // Move to the next buffer if current buffer is processed
  }

  if (length > 0) log_cache->truncate(log_cache->length());

  return tv_event;
}

static uint32 abstract_event_timestamp_from_buffer(uchar *buf, size_t len) {
  uchar *header = buf;
  uint32 tv_event = 0;
  size_t event_len = 0;
  DBUG_TRACE;

  // Loop through the buffer
  while ((size_t)(header - buf) < len) {
    assert(header + EVENT_LEN_OFFSET <= buf + len);

    tv_event = uint4korr(header);
    if (tv_event > 0) break;

    event_len = uint4korr(header + EVENT_LEN_OFFSET);
    header += event_len;  // Move to the next event in the buffer
  }

  return tv_event;
}

static bool write_consensus_log_event(MYSQL_BIN_LOG::Binlog_ofile *binlog_file,
                                      uint flag, uint64 term, uint64 length,
                                      uint64 checksum, bool consensus_append) {
  DBUG_TRACE;
  Consensus_log_event rev(flag, term, consensus_log_manager.get_current_index(),
                          length);
  if (consensus_append && consensus_log_manager.get_event_timestamp() > 0)
    rev.common_header->when.tv_sec = consensus_log_manager.get_event_timestamp();
  rev.common_header->log_pos = binlog_file_get_current_pos(binlog_file);
  rev.common_footer->checksum_alg =
      static_cast<enum_binlog_checksum_alg>(binlog_checksum_options);
  rev.set_reserve(checksum);
  if (!(rev.get_flag() & Consensus_log_event_flag::FLAG_LARGE_TRX))
    rpl_consensus_set_last_noncommit_dep_index(rev.get_index());

  if (binary_event_serialize(&rev, (Basic_ostream *)binlog_file)) return true;

  if (!consensus_append && rev.common_header->when.tv_sec > 0)
    consensus_log_manager.set_event_timestamp(rev.common_header->when.tv_sec);
  consensus_log_manager.incr_current_index();

  return false;
}

static int write_cache_consensus_log(THD *thd, Gtid_log_event &gtid_event,
                                     binlog_cache_data *cache_data,
                                     MYSQL_BIN_LOG::Binlog_ofile *binlog_file,
                                     bool have_checksum) {
  int error = 0;
  uint flag = 0;
  bool is_large_trx = false;
  DBUG_TRACE;

  Binlog_cache_storage *cache_storage = binlog_cache_get_storage(cache_data);
  ulonglong total_size =
      cache_storage->length() +       /* binlog cache data */
      gtid_event.get_event_length() + /* gtid event */
      (have_checksum ? (binlog_cache_get_event_counter(cache_data) + 1) *
                           BINLOG_CHECKSUM_LEN
                     : 0); /* checksum for each event including gtid event */

  // determine whether log is too large
  if (total_size > opt_consensus_max_log_size) is_large_trx = true;

  /* Check large trx */
  if (!opt_consensus_large_trx && is_large_trx) {
    sql_print_warning(
        "Failed to flush log ,because consensus log is too large.");
    thd->mark_transaction_to_rollback(true);
    thd->consensus_context.consensus_error =
        Consensus_binlog_context_info::CSS_LOG_TOO_LARGE;
    return 0;
  }

  /* Check server status */
  mysql_mutex_lock(consensus_log_manager.get_log_term_lock());
  if (rpl_consensus_log_get_term() != thd->consensus_context.consensus_term ||
      rpl_consensus_get_term() != thd->consensus_context.consensus_term) {
    sql_print_warning(
        "Failed to flush log ,because consensus leader is changed.");
    thd->mark_transaction_to_rollback(true);
    thd->consensus_context.consensus_error =
        Consensus_binlog_context_info::CSS_LEADERSHIP_CHANGE;
    goto end;
  }

  DBUG_EXECUTE_IF("force_large_trx", { is_large_trx = true; });
  if (!is_large_trx) {
    uint32 crc32 = 0;
    if (consensus_log_manager.get_first_event_in_file()) {
      flag |= Consensus_log_event_flag::FLAG_ROTATE;
      consensus_log_manager.set_first_event_in_file(false);
    }
    /* write_consensus_log_event will advance current_index */
    thd->consensus_context.consensus_index =
        consensus_log_manager.get_current_index();

    /**
     * Write gtid and binlog cache data to consensus log cache with
     * right log_end_pos
     */
    Consensuslog_event_writer consensus_writer(
        consensus_log_manager.get_log_cache(), have_checksum,
        binlog_file_get_current_pos(binlog_file));
    consensus_log_manager.get_log_cache()->reset();
    consensus_writer.inc_end_log_pos(Consensus_log_event::get_event_length() +
                                     (have_checksum ? BINLOG_CHECKSUM_LEN : 0));

    if (gtid_event.write(&consensus_writer) ||
        cache_storage->copy_to(&consensus_writer)) {
      error = 1;
      goto end;
    }

    size_t payload_size = consensus_log_manager.get_log_cache()->length();
    if (opt_consensus_checksum &&
        calc_consensus_crc(consensus_log_manager.get_log_cache(), crc32)) {
      error = 1;
      goto end;
    }

    /* Write consensus log event */
    if (write_consensus_log_event(binlog_file, flag,
                                  thd->consensus_context.consensus_term,
                                  payload_size, crc32)) {
      error = 1;
      goto end;
    }

    /*  Write gtid log event and binlog cache data */
    if (stream_copy(consensus_log_manager.get_log_cache(),
                    (Basic_ostream *)binlog_file)) {
      error = 1;
      goto end;
    }

    uchar *payload = (uchar *)my_malloc(key_memory_thd_main_mem_root,
                                        (size_t)payload_size, MYF(MY_WME));
    copy_from_consensus_log_cache(consensus_log_manager.get_log_cache(),
                                  payload, payload_size);
    if (consensus_log_manager.get_fifo_cache_manager()->add_log_to_cache(
            thd->consensus_context.consensus_term,
            thd->consensus_context.consensus_index, payload_size, payload,
            false, flag, crc32, true) == 1) {
      my_free(payload);
    }
    payload = NULL;
  } else {
    error = large_trx_flush(thd, total_size, have_checksum, gtid_event,
                            cache_data, binlog_file);
  }

end:
  mysql_mutex_unlock(consensus_log_manager.get_log_term_lock());
  return error;
}

static int append_consensue_log_entry(ConsensusLogEntry &log,
                                      MYSQL_BIN_LOG::Binlog_ofile *binlog_file,
                                      bool &add_cache, Relay_log_info *rli) {
  int error = 0;
  DBUG_TRACE;

  ulonglong payload_start_pos =
      binlog_file_get_current_pos(binlog_file) +
      (Consensus_log_event::get_event_length() +
       (binlog_checksum_options != binary_log::BINLOG_CHECKSUM_ALG_OFF
            ? BINLOG_CHECKSUM_LEN
            : 0));

  sql_print_information(
      "append_consensue_log_entry: the current pos(%lu), "
      "payload_start_pos(%lu)",
      binlog_file_get_current_pos(binlog_file), payload_start_pos);

  add_cache = true;

  if (log.outer) {
    Consensuslog_event_writer consensus_cache_writer(
        consensus_log_manager.get_log_cache(),
        binlog_checksum_options != binary_log::BINLOG_CHECKSUM_ALG_OFF,
        payload_start_pos);

    consensus_log_manager.get_log_cache()->reset();

    Consensus_cluster_info_log_event ev(log.buf_size, (char *)(log.buffer));
    ev.common_footer->checksum_alg =
        static_cast<enum_binlog_checksum_alg>(binlog_checksum_options);

    if (rli)
      ev.common_header->when.tv_sec =
          consensus_log_manager.get_event_timestamp();

    ev.set_relay_log_event();
    error = ev.write(&consensus_cache_writer);

    /* Recalcute the checksum because the log_end_pos is changed */
    uint32 checksum;
    if (!error && opt_consensus_checksum) {
      if (!calc_consensus_crc(consensus_log_manager.get_log_cache(),
                              checksum)) {
        sql_print_error(
            "append_consensue_log_entry: failed to calc consensus entry crc");
        error = 1;
      } else {
        log.checksum = checksum;
      }
    }

    if (!error) {
      if (write_consensus_log_event(
              binlog_file, log.flag, log.term,
              consensus_log_manager.get_log_cache()->length(), log.checksum,
              true)) {
        sql_print_error(
            "append_consensue_log_entry: failed to write consensus log event");
        error = 1;
      } else if (stream_copy(consensus_log_manager.get_log_cache(),
                             (Basic_ostream *)binlog_file)) {
        sql_print_error(
            "append_consensue_log_entry: failed to copy consensus log cache to "
            "binlog file");
        error = 1;
      }
    }
  } else if (log.flag & Consensus_log_event_flag::FLAG_BLOB) {
    Consensuslog_event_writer empty_log_writer(
        (Basic_ostream *)binlog_file,
        binlog_checksum_options != binary_log::BINLOG_CHECKSUM_ALG_OFF,
        payload_start_pos, true, true);

    if (log.flag & Consensus_log_event_flag::FLAG_BLOB_START) {
      consensus_log_manager.get_log_cache()->reset();
    }

    sql_print_information("Large event: cache the current log, size(%lu).",
                          log.buf_size);

    /* Save real data to cache */
    if (consensus_log_manager.get_log_cache()->write(log.buffer,
                                                      log.buf_size)) {
      sql_print_error(
          "append_consensue_log_entry: failed to write consensus log cache");
      error = 1;
    } else {
      std::string empty_log = consensus_log_manager.get_empty_log();
      int4store((uchar *)empty_log.c_str(),
                consensus_log_manager.get_event_timestamp());

      /*
       * Consensus entry checksum could not be updated at here, because the
       * payload event's real log_end_pos is unkown. Recompute the checksum when
       * reading
       */
      if (write_consensus_log_event(binlog_file, log.flag, log.term,
                                    empty_log.length(), log.checksum, true)) {
        sql_print_error(
            "append_consensue_log_entry: failed to write consensus log event");
        error = 1;
      } else if (empty_log_writer.write((const uchar *)empty_log.c_str(),
                                        empty_log.length())) {
        sql_print_error(
            "append_consensue_log_entry: failed to write empty consensus log "
            "event");
        error = 1;
      }
    }

    if (opt_consensus_checksum) add_cache = false;
  } else if (log.flag & Consensus_log_event_flag::FLAG_BLOB_END) {
    Consensuslog_event_writer consensus_log_writer(
        (Basic_ostream *)binlog_file,
        binlog_checksum_options != binary_log::BINLOG_CHECKSUM_ALG_OFF,
        payload_start_pos, true, true);

    sql_print_information("Large event: cache the current log, size(%lu).",
                          log.buf_size);

    if (consensus_log_manager.get_log_cache()->write(log.buffer,
                                                     log.buf_size)) {
      sql_print_error(
          "append_consensue_log_entry: failed to write consensus log cache");
      error = 1;
    } else {
      DBUG_EXECUTE_IF("crash_during_large_event_receive", { DBUG_SUICIDE(); });
      DBUG_EXECUTE_IF("crash_during_large_event_receive_slow", {
        sleep(2);
        DBUG_SUICIDE();
      });

      uint32 ev_ts = abstract_event_timestamp_from_cache(
          consensus_log_manager.get_log_cache());
      if (ev_ts > 0) consensus_log_manager.set_event_timestamp(ev_ts);

      sql_print_information(
          "Large event end: write the whole event, size(%lu).",
          consensus_log_manager.get_log_cache()->length());

      /* Write the whole event to binlog file */
      if (write_consensus_log_event(
              binlog_file, log.flag, log.term,
              consensus_log_manager.get_log_cache()->length(), log.checksum,
              true)) {
        sql_print_error(
            "append_consensue_log_entry: failed to write consensus log event");
        error = 1;
      } else if (stream_copy(consensus_log_manager.get_log_cache(),
                             &consensus_log_writer)) {
        sql_print_error(
            "append_consensue_log_entry: failed to copy consensus log "
            "cache to binlog file");
        error = 1;
      }
    }

    consensus_log_manager.get_log_cache()->reset();

    if (opt_consensus_checksum) add_cache = false;
  } else {
    /* Revise log event */
    Consensuslog_event_writer payload_reviser(
        nullptr, binlog_checksum_options != binary_log::BINLOG_CHECKSUM_ALG_OFF,
        payload_start_pos, false, true);
    if (payload_reviser.revise_buffer((uchar *)log.buffer, log.buf_size)) {
      sql_print_error(
          "append_consensue_log_entry: failed to revise consensus log");
      error = 1;
    } else {
      uint32 ev_ts =
          abstract_event_timestamp_from_buffer(log.buffer, log.buf_size);
      if (ev_ts > 0) consensus_log_manager.set_event_timestamp(ev_ts);

      /* Recompute crc for revised event */
      log.checksum = opt_consensus_checksum ? binary_log::checksum_crc32(
                                                  0, log.buffer, log.buf_size)
                                            : 0;
      /* Write the revised event to binlog file */
      if (write_consensus_log_event(binlog_file, log.flag, log.term,
                                    log.buf_size, log.checksum, true)) {
        sql_print_error(
            "append_consensue_log_entry: failed to write consensus log event");
        error = 1;
      } else if (write_buffer_to_binlog_file(
                     binlog_file, (const uchar *)log.buffer, log.buf_size)) {
        sql_print_error(
            "append_consensue_log_entry: failed to write payload to binlog "
            "file");
        error = 1;
      } else if (rli != nullptr) {
        store_gtid_for_consensus_log((const char *)log.buffer, rli);
      }
    }
  }

  return error;
}

int append_consensus_log(MYSQL_BIN_LOG *binlog, ConsensusLogEntry &log,
                         uint64 *index, Relay_log_info *rli, bool with_check) {
  int error = 0;
  DBUG_TRACE;

  mysql_mutex_lock(binlog->get_log_lock());
  bool add_to_cache = true;
  if (with_check) {
    mysql_mutex_lock(consensus_log_manager.get_log_term_lock());
    if (rpl_consensus_log_get_term() != log.term) {
      mysql_mutex_unlock(consensus_log_manager.get_log_term_lock());
      mysql_mutex_unlock(binlog->get_log_lock());
      /* set index to 0 to mark it fail */
      *index = 0;
      /* return 0 do not let it abort */
      return 0;
    }
    mysql_mutex_unlock(consensus_log_manager.get_log_term_lock());
  }

  *index = consensus_log_manager.get_current_index();
  if (*index != log.index &&
      log.index != 0)  // leader write empty log entry with index 0
  {
    sql_print_error(
        "Consensus Index mismatch, system current index is %llu, "
        "but the log index is %llu.",
        *index, log.index);
    mysql_mutex_unlock(binlog->get_log_lock());
    *index = 0;
    return 0;
  }

  sql_print_information("append_consensus_log: the current index(%lu)", *index);

  error = append_consensue_log_entry(log, binlog->get_binlog_file(),
                                     add_to_cache, rli);

  if (!error) error = binlog->flush_and_sync(false);

  if (error) goto err;

  if (add_to_cache)
    consensus_log_manager.get_fifo_cache_manager()->add_log_to_cache(
        log.term, *index, log.buf_size, log.buffer, log.outer, log.flag,
        log.checksum);

  consensus_log_manager.set_sync_index_if_greater(*index);

  binlog->update_binlog_end_pos();

  if (opt_cluster_log_type_instance) rpl_consensus_update_applied_index(*index);
err:
  mysql_mutex_unlock(binlog->get_log_lock());
  return error;
}

int append_multi_consensus_logs(MYSQL_BIN_LOG *binlog,
                                std::vector<ConsensusLogEntry> &logs,
                                uint64 *max_index, Relay_log_info *rli) {
  int error = 0;
  bool add_to_cache = true;
  uint64 flush_index = 0;
  DBUG_TRACE;

  mysql_mutex_lock(binlog->get_log_lock());

  for (auto iter = logs.begin(); iter != logs.end(); iter++) {
    if (consensus_log_manager.get_current_index() != iter->index) {
      sql_print_error(
          "Consensus Index Mismatch, system current index is %llu, "
          "but the log index is %llu.",
          consensus_log_manager.get_current_index(), iter->index);
      *max_index = 0;
      break;
    }

    flush_index = consensus_log_manager.get_current_index();
    sql_print_information("append_multi_consensus_logs: the current index(%lu)",
                          flush_index);

    error = append_consensue_log_entry(*iter, binlog->get_binlog_file(),
                                       add_to_cache, rli);

    if (error) {
      sql_print_error(
          "append_multi_consensus_logs: write consensus log entry %ld error",
          flush_index);
      break;
    }

    if (add_to_cache)
      consensus_log_manager.get_fifo_cache_manager()->add_log_to_cache(
          iter->term, flush_index, iter->buf_size, iter->buffer, iter->outer,
          iter->flag, iter->checksum);

    *max_index = flush_index;
  }

  if (!error) error = binlog->flush_and_sync(false);

  if (error) goto err;

  consensus_log_manager.set_sync_index_if_greater(flush_index);

  binlog->update_binlog_end_pos();

  if (opt_cluster_log_type_instance)
    rpl_consensus_update_applied_index(flush_index);

err:
  mysql_mutex_unlock(binlog->get_log_lock());
  return error;
}

static int add_to_consensus_log_file_index(
    std::vector<std::string> consensuslog_file_name_vector,
    ConsensusLogIndex *log_file_index, bool remove_dup = false,
    ulong stop_datetime = 0) {
  bool reached_stop_point = false;
  DBUG_TRACE;

  for (auto iter = consensuslog_file_name_vector.begin();
       !reached_stop_point && iter != consensuslog_file_name_vector.end();
       ++iter) {
    Binlog_file_reader binlog_file_reader(opt_source_verify_checksum);
    if (binlog_file_reader.open(iter->c_str())) {
      LogErr(ERROR_LEVEL, ER_BINLOG_FILE_OPEN_FAILED,
             binlog_file_reader.get_error_str());
      return 1;
    }
    binlog_file_reader.seek(BIN_LOG_HEADER_SIZE);

    Log_event *ev = NULL;
    bool find_prev_consensus_log = false;

    while (!find_prev_consensus_log &&
           (ev = binlog_file_reader.read_event_object()) != NULL) {
      switch (ev->get_type_code()) {
        case binary_log::PREVIOUS_CONSENSUS_INDEX_LOG_EVENT: {
          Previous_consensus_index_log_event *prev_consensus_index_ev =
              (Previous_consensus_index_log_event *)ev;
          if (stop_datetime > 0 &&
              (ulong)prev_consensus_index_ev->common_header->when.tv_sec >
                  stop_datetime) {
            reached_stop_point = true;
          } else {
            log_file_index->add_to_index_list(
                prev_consensus_index_ev->get_index(),
                prev_consensus_index_ev->common_header->when.tv_sec, *iter,
                0 /* binlog_file_reader.position() */, remove_dup);
          }
          find_prev_consensus_log = true;
          break;
        }
        default:
          break;
      }
      delete ev;
    }

    if (!find_prev_consensus_log) {
      sql_print_error("log file %s do not contain prev_consensus_log_ev",
                      iter->c_str());
      return 1;
    }
  }

  return 0;
}

int build_consensus_log_index(MYSQL_BIN_LOG *binlog) {
  int error = 0;
  std::vector<std::string> consensuslog_file_name_vector;
  DBUG_TRACE;

  error = binlog->get_file_names(consensuslog_file_name_vector);
  if (error) return error;

  error = add_to_consensus_log_file_index(
      consensuslog_file_name_vector, consensus_log_manager.get_log_file_index(),
      false);
  if (error) return error;

  return 0;
}

int write_cache_to_consensus_log(THD *thd, MYSQL_BIN_LOG *binlog,
                                 Gtid_log_event &gtid_event,
                                 binlog_cache_data *cache_data,
                                 bool have_checksum) {
  DBUG_TRACE;
  Binlog_cache_storage *cache = binlog_cache_get_storage(cache_data);

  mysql_mutex_assert_owner(binlog->get_log_lock());

  assert(binlog->is_open());
  if (likely(binlog->is_open()))  // Should always be true
  {
    if (!cache->is_empty()) {
      if (write_cache_consensus_log(thd, gtid_event, cache_data,
                                    binlog->get_binlog_file(), have_checksum))
        goto err;

      // TODO: wesql cluster, handle incident event

      DBUG_EXECUTE_IF("half_binlogged_transaction", DBUG_SUICIDE(););
    }
    binlog->update_thd_next_event_pos(thd);
  }
  // update stats if monitoring is active
  update_trx_compression(cache_data, thd->owned_gtid, gtid_event.immediate_commit_timestamp);
  return false;
err:
  return true;
}

int rotate_consensus_log(THD *thd, bool force_rotate) {
  int error = 0;
  DBUG_TRACE;

  DBUG_EXECUTE_IF("crash_before_rotate_consensus_log", DBUG_SUICIDE(););
  DEBUG_SYNC(thd, "before_rotate_consensus_log");
  mysql_rwlock_rdlock(consensus_log_manager.get_consensuslog_status_lock());
  MYSQL_BIN_LOG *binlog =
      consensus_log_manager.get_status() == BINLOG_WORKING
          ? consensus_log_manager.get_binlog()
          : &consensus_log_manager.get_relay_log_info()->relay_log;

  /* do not rotate in middle of a large trx */
  if (consensus_log_manager.get_status() != BINLOG_WORKING &&
      !consensus_log_manager.get_enable_rotate()) {
    mysql_rwlock_unlock(consensus_log_manager.get_consensuslog_status_lock());
    sql_print_error(
        "Fail to rotate log because the last consensus log entry is in "
        "middle of a large transaction.");
    my_error(ER_CONSENSUS_FOLLOWER_NOT_ALLOWED, MYF(0));
    error = 1;
    goto err;
  }

  error = binlog->rotate_and_purge(thd, force_rotate);

  mysql_rwlock_unlock(consensus_log_manager.get_consensuslog_status_lock());

err:
  return error;
}

int reencrypt_consensus_logs() {
  int error = 0;
  DBUG_TRACE;

  mysql_rwlock_rdlock(consensus_log_manager.get_consensuslog_status_lock());

  MYSQL_BIN_LOG *binlog =
      consensus_log_manager.get_status() == BINLOG_WORKING
          ? consensus_log_manager.get_binlog()
          : &consensus_log_manager.get_relay_log_info()->relay_log;

  error = binlog->reencrypt_logs();

  mysql_rwlock_unlock(consensus_log_manager.get_consensuslog_status_lock());
  return error;
}

int purge_consensus_logs_on_conditions(ulong purge_time, ulong purge_size,
                                       const char *to_log, bool auto_purge) {
  uint64 target_index = 0;
  int res = 0;
  DBUG_TRACE;

  if (purge_time > 0) {
    std::string log_name;
    // get the log index by target time
    consensus_log_manager.get_log_file_index()->
          get_first_log_should_purge_by_time(purge_time, log_name, target_index);
    sql_print_information(
        "Consensus purge logs before time: %d, log: %s, "
        "target_index:%llu",
        purge_time, log_name.c_str(), target_index);
  } else if (to_log != nullptr) {
    target_index =
        consensus_log_manager.get_log_file_index()->get_start_index_of_file(
            to_log);
    sql_print_information(
        "Consensus purge logs before file: %s, target_index: %llu", to_log,
        target_index);
  } else if (purge_size > 0) {
    // TODO: purge logs by total size.
  }

  if (target_index > 0) {
    res = rpl_consensus_force_purge_log(auto_purge /* local */, target_index);
  }

  return res;
}

static int consensus_truncate_all_logs(MYSQL_BIN_LOG *log) {
  DBUG_TRACE;
  int error = log->truncate_all_files();

  if (!error) consensus_log_manager.get_log_file_index()->clear_all();

  return error;
}

static int consensus_truncate_log(MYSQL_BIN_LOG *log, uint64 consensus_index) {
  int error = 0;
  std::string file_name;
  uint64 start_index = 0;
  uint64 offset;
  DBUG_TRACE;

  mysql_mutex_assert_owner(log->get_log_lock());

  if (consensus_find_log_by_index(consensus_index, file_name, start_index) ||
      consensus_find_pos_by_index(file_name.c_str(), start_index,
                                  consensus_index, &offset)) {
    error = 1;
  } else if (log->truncate_log(file_name.c_str(), offset, nullptr)) {
    error = 1;
  }

  if (!error) {
    consensus_log_manager.set_sync_index(consensus_index - 1);
    consensus_log_manager.set_current_index(consensus_index);
    consensus_log_manager.get_log_file_index()->truncate_pos_map_of_file(
        start_index, consensus_index);
  } else {
    abort();
  }

  return error;
}


/**
  Converts a datetime String value to its my_time_t representation.

  @retval 0	OK
  @retval =!0	error
*/
static int convert_archive_stop_datetime(const char *str, ulong &my_time) {
  MYSQL_TIME_STATUS status;
  MYSQL_TIME l_time;
  bool dummy_in_dst_time_gap;
  DBUG_TRACE;

  /* We require a total specification (date AND time) */
  if (str_to_datetime(str, strlen(str), &l_time, 0, &status) ||
      (l_time.time_type != MYSQL_TIMESTAMP_DATETIME_TZ &&
       l_time.time_type != MYSQL_TIMESTAMP_DATETIME) ||
      status.warnings) {
    return 1;
  }

  /*
    Note that Feb 30th, Apr 31st cause no error messages and are mapped to
    the next existing day, like in mysqld. Maybe this could be changed when
    mysqld is changed too (with its "strict" mode?).
  */
  my_time = (ulong)my_tz_SYSTEM->TIME_to_gmt_sec(&l_time, &dummy_in_dst_time_gap);

  return 0;
}

/**
  Get the next index from binlog file

  - retrieve only the end of large transactions

  @retval 0	ok
  @retval 1	error
*/
static int consensus_get_next_index(const char *file_name,
                                    const ulong stop_datetime,
                                    bool &reached_stop_point,
                                    uint64 &current_term) {
  Log_event *ev = NULL;
  Consensus_log_event *consensus_log_ev = NULL;
  Previous_consensus_index_log_event *consensus_prev_ev = NULL;
  uint64 next_index = 0;
  uint64 current_flag = 0;
  DBUG_TRACE;

  Binlog_file_reader binlog_file_reader(opt_source_verify_checksum);
  if (binlog_file_reader.open(file_name)) {
    LogErr(ERROR_LEVEL, ER_BINLOG_FILE_OPEN_FAILED,
           binlog_file_reader.get_error_str());
    return 0;
  }
  binlog_file_reader.seek(BIN_LOG_HEADER_SIZE);

  reached_stop_point = false;
  while (!reached_stop_point &&
         (ev = binlog_file_reader.read_event_object()) != NULL) {
    switch (ev->get_type_code()) {
      case binary_log::PREVIOUS_CONSENSUS_INDEX_LOG_EVENT:
        consensus_prev_ev = (Previous_consensus_index_log_event *)ev;
        next_index = consensus_prev_ev->get_index();
        break;
      case binary_log::CONSENSUS_LOG_EVENT:
        consensus_log_ev = (Consensus_log_event *)ev;
        current_flag = consensus_log_ev->get_flag();
        current_term = consensus_log_ev->get_term();
        if (stop_datetime > 0 &&
            (ulong)consensus_log_ev->common_header->when.tv_sec >
                stop_datetime) {
          reached_stop_point = true;
        } else if (!(current_flag & Consensus_log_event_flag::FLAG_LARGE_TRX)) {
          next_index = consensus_log_ev->get_index() + 1;
        }
        break;
      default:
        break;
    }
    delete ev;
  }

  return next_index;
}

/**
  Generate new binlog files from archive

  - retrieve all archive files from archive log index
  - skip duplicate binlog event

  @retval 0	OK
  @retval !=0	error
*/
int consensus_open_archive_log(uint64 first_index, uint64 last_index) {
  int error = 0;
  std::vector<std::string> consensus_file_name_vector;
  std::vector<ConsensusLogIndexEntry> consensus_file_entry_vector;
  char log_dirname[FN_REFLEN], last_log_dirname[FN_REFLEN];
  size_t log_dirname_len = 0, last_log_dirname_len = 0;
  Relay_log_info *rli_info = consensus_log_manager.get_relay_log_info();
  MYSQL_BIN_LOG *relay_log = &rli_info->relay_log;
  uint64 next_index = last_index + 1;
  uint64 last_term = 0;
  ConsensusLogIndex consensus_log_index;
  bool add_new_file = false;
  ulong stop_datetime = 0;
  bool reached_stop_point = false;
  DBUG_TRACE;

  MYSQL_BIN_LOG archive_log(nullptr, true);
#ifdef HAVE_PSI_INTERFACE
  archive_log.set_psi_keys(
      key_RELAYLOG_LOCK_index, key_RELAYLOG_LOCK_commit, PSI_NOT_INSTRUMENTED,
      PSI_NOT_INSTRUMENTED, PSI_NOT_INSTRUMENTED, PSI_NOT_INSTRUMENTED,
      PSI_NOT_INSTRUMENTED, key_RELAYLOG_LOCK_log,
      key_RELAYLOG_LOCK_log_end_pos, key_RELAYLOG_LOCK_sync,
      PSI_NOT_INSTRUMENTED, key_RELAYLOG_LOCK_xids, PSI_NOT_INSTRUMENTED,
      PSI_NOT_INSTRUMENTED, PSI_NOT_INSTRUMENTED, PSI_NOT_INSTRUMENTED,
      key_RELAYLOG_update_cond, PSI_NOT_INSTRUMENTED, PSI_NOT_INSTRUMENTED,
      key_file_relaylog, key_file_relaylog_index, key_file_relaylog_cache,
      key_file_relaylog_index_cache);
#endif
  archive_log.init_pthread_objects();

  sql_print_information(
      "Starting archive recovery from %s, last log index: %llu, recovery "
      "target time: %s",
      opt_archive_log_index_name, last_index,
      opt_archive_recovery_stop_datetime_str);

  consensus_log_index.init();

  if (opt_archive_recovery_stop_datetime_str != nullptr &&
      convert_archive_stop_datetime(opt_archive_recovery_stop_datetime_str,
                                    stop_datetime)) {
    consensus_log_index.cleanup();
    LogErr(ERROR_LEVEL, ER_WRONG_DATETIME_SPEC,
           opt_archive_recovery_stop_datetime_str);
    return 1;
  }

  /* Skip archived recovery if archive-log-index is not valid */
  if (archive_log.open_index_file(opt_archive_log_index_name, nullptr, true)) {
    sql_print_warning("Failed to open archive log index file %s",
                      opt_archive_log_index_name);
    consensus_log_index.cleanup();
    return 0;
  }

  /* Build archived log index */
  if (archive_log.get_file_names(consensus_file_name_vector) ||
      add_to_consensus_log_file_index(consensus_file_name_vector,
                                      &consensus_log_index, true,
                                      stop_datetime)) {
    archive_log.close();
    consensus_log_index.cleanup();
    return 1;
  }

  mysql_mutex_lock(relay_log->get_log_lock());

  consensus_log_index.get_log_file_entry_list(consensus_file_entry_vector);

  auto file_iter = consensus_file_entry_vector.begin();
  if (file_iter != consensus_file_entry_vector.end())
    dirname_part(log_dirname, file_iter->file_name.c_str(), &log_dirname_len);

  for (; !reached_stop_point && file_iter != consensus_file_entry_vector.end();
       file_iter++) {
    uint64 file_next_index = 0;
    auto next_file_iter = std::next(file_iter);

    sql_print_information(
        "Processing archive log file %s with start index %llu, The expected "
        "next index is %llu",
        file_iter->file_name.c_str(), file_iter->index, next_index);

    if (next_index < file_iter->index) {
      sql_print_error(
          "The archive log file %s with start index %llu is not continuous "
          "with the previous log. The expected next index is %llu",
          file_iter->file_name.c_str(), file_iter->index, next_index);
      error = 1;
      goto ret;
    } else if (next_index > file_iter->index) {
      /* Skip current archive file if it's covered by next file */
      if (next_file_iter != consensus_file_entry_vector.end()) {
        file_next_index = next_file_iter->index;
      } else {
        // Get the stop(next) index for last archive file
        file_next_index = consensus_get_next_index(
            file_iter->file_name.c_str(), stop_datetime, reached_stop_point,
            last_term);
      }
      if (file_next_index == 0) {
        LogErr(ERROR_LEVEL, ER_CONSENSUS_GET_NEXT_INDEX_FAILED,
               file_iter->file_name.c_str());
        error = 1;
        goto ret;
      }

      if (next_index >= file_next_index) {
        /* [file_iter->index, current_end_index] had been covered */
        sql_print_information(
            "Ignored archive binlog file %s. Its end index %llu is less than "
            "%llu as expected.",
            file_iter->file_name.c_str(), file_next_index - 1, next_index);
        continue;
      }

      sql_print_information("Truncating consensus binlog to index %llu.",
                            file_iter->index);

      /* Truncate duplicate binlog events from last file */
      if (file_iter->index < first_index) {
        if ((error = consensus_truncate_all_logs(relay_log))) goto ret;
      } else {
        if ((error = consensus_truncate_log(relay_log, file_iter->index)))
          goto ret;
      }
    }

    sql_print_information(
        "Generation a new binlog file from archive binlog file %s.",
        file_iter->file_name.c_str());

    if ((error = relay_log->new_relay_log_from_archive(
             file_iter->file_name.c_str(), false)))
      goto ret;

    consensus_log_manager.get_log_file_index()->add_to_index_list(
        consensus_log_manager.get_current_index(), file_iter->timestamp,
        file_iter->file_name,
        0 /* binlog_file_get_current_pos(relay_log->get_binlog_file()) */,
        false);

    add_new_file = true;

    strmake(last_log_dirname, log_dirname, log_dirname_len);
    last_log_dirname_len = log_dirname_len;

    /* Get the next index of current archive file */
    if (next_file_iter != consensus_file_entry_vector.end()) {
      dirname_part(log_dirname, next_file_iter->file_name.c_str(),
                   &log_dirname_len);
      if (log_dirname_len == last_log_dirname_len &&
          0 == strcmp(log_dirname, last_log_dirname)) {
        next_index = next_file_iter->index;
      } else {
        // Get the stop(next) index for last archive file
        next_index = consensus_get_next_index(file_iter->file_name.c_str(),
                                              stop_datetime, reached_stop_point,
                                              last_term);
      }
    } else if (file_next_index != 0) {
      next_index = file_next_index;
    } else {
      // Get the stop(next) index for last archive file
      next_index =
          consensus_get_next_index(file_iter->file_name.c_str(), stop_datetime,
                                   reached_stop_point, last_term);
    }

    if (next_index == 0) {
      LogErr(ERROR_LEVEL, ER_CONSENSUS_GET_NEXT_INDEX_FAILED,
             next_file_iter->file_name.c_str());
      error = 1;
      goto ret;
    }

    sql_print_information(
        "The next index of current archive binlog file %s is %llu.",
        file_iter->file_name.c_str(), next_index);

    /* Truncate binlog events after stop timestamp */
    if (stop_datetime > 0 && reached_stop_point) {
      sql_print_information(
          "Reached stop point of archive recovery. The next index is %llu.",
          next_index);
      if ((error = consensus_truncate_log(relay_log, next_index))) goto ret;
    }

    consensus_log_manager.set_cache_index(next_index - 1);
    consensus_log_manager.set_sync_index(next_index - 1);
    consensus_log_manager.set_current_index(next_index);
  }
  if (last_term > 0) consensus_log_manager.set_current_term(last_term);

ret:
  mysql_mutex_unlock(relay_log->get_log_lock());
  archive_log.close();
  consensus_log_index.cleanup();
  if (!error && add_new_file) {
    error = rli_info->reset_previous_gtid_set_of_consensus_log();
  }
  return error;
}