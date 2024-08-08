#include "consensus_state_process.h"
#include "consensus_applier.h"
#include "consensus_binlog.h"
#include "consensus_binlog_recovery.h"
#include "consensus_log_manager.h"
#include "consensus_meta.h"
#include "consensus_recovery_manager.h"
#include "plugin.h"
#include "plugin_psi.h"
#include "rpl_consensus.h"
#include "system_variables.h"

#include "mysql/psi/mysql_file.h"

#include "my_loglevel.h"
#include "mysql/components/services/log_builtins.h"
#include "mysqld_error.h"
#include "sql/log.h"

#include "sql/consensus/consensus_info_factory.h"
#include "sql/mysqld.h"
#include "sql/rpl_info_factory.h"
#include "sql/rpl_mi.h"
#include "sql/rpl_msr.h"
#include "sql/rpl_replica.h"
#include "sql/xa.h"

using binary_log::checksum_crc32;
using std::max;

ConsensusStateProcess consensus_state_process;

ConsensusStateProcess::ConsensusStateProcess()
    : inited(false),
      current_term(1),
      current_state_degrade_term(0),
      recovery_index_hwl(0),
      status(Consensus_Log_System_Status::BINLOG_WORKING),
      binlog(nullptr),
      rli_info(nullptr),
      consensus_state_change_is_running(false) {}

ConsensusStateProcess::~ConsensusStateProcess() {}

int ConsensusStateProcess::init() {
  mysql_rwlock_init(key_rwlock_ConsensusLog_status_lock,
                    &LOCK_consensuslog_status);
  mysql_rwlock_init(key_rwlock_ConsensusLog_commit_lock,
                    &LOCK_consensuslog_commit);
  mysql_mutex_init(key_mutex_ConsensusLog_term_lock, &LOCK_consensuslog_term,
                   MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_mutex_Consensus_stage_change,
                   &LOCK_consensus_state_change, MY_MUTEX_INIT_FAST);

  mysql_cond_init(key_COND_Consensus_state_change,
                  &COND_consensus_state_change);

  status = Consensus_Log_System_Status::BINLOG_WORKING;

  inited = true;
  return 0;
}

void stateChangeCb(StateType state, uint64_t term, uint64_t commitIndex) {
  ConsensusStateChange state_change = {state, term, commitIndex};
  consensus_state_process.add_state_change_request(state_change);
}

uint32 binlog_checksum_crc32_callback(uint32 crc, const unsigned char *pos,
                                      size_t length) {
  return checksum_crc32(crc, pos, length);
}

static int reset_previous_logged_gtids_relaylog(const Gtid_set *gtid_set,
                                                Checkable_rwlock *sid_lock) {
  DBUG_TRACE;
  const Gtid_set *executed_gtids = gtid_state->get_executed_gtids();
  const Gtid_set *gtids_only_in_table = gtid_state->get_gtids_only_in_table();

  sid_lock->wrlock();

  executed_gtids->get_sid_map()->get_sid_lock()->wrlock();
  int ret = (const_cast<Gtid_set *>(gtid_set)->add_gtid_set(executed_gtids) !=
             RETURN_STATUS_OK);
  executed_gtids->get_sid_map()->get_sid_lock()->unlock();

  if (!ret) {
    gtids_only_in_table->get_sid_map()->get_sid_lock()->wrlock();
    const_cast<Gtid_set *>(gtid_set)->remove_gtid_set(gtids_only_in_table);
    gtids_only_in_table->get_sid_map()->get_sid_lock()->unlock();
  }

  sid_lock->unlock();

  return ret;
}

MYSQL_BIN_LOG *ConsensusStateProcess::get_consensus_log() {
  return status == Consensus_Log_System_Status::BINLOG_WORKING
             ? binlog
             : &rli_info->relay_log;
}

int ConsensusStateProcess::wait_leader_degraded(uint64 term, uint64 index) {
  int error = 0;
  DBUG_TRACE;

  LogPluginErr(SYSTEM_LEVEL, ER_CONSENSUS_STATE_LEADER_DEGRADED, "started",
               term, index);

  DBUG_EXECUTE_IF("simulate_leader_degrade_slow", {
    my_sleep(10000000);
    LogPluginErr(SYSTEM_LEVEL, ER_CONSENSUS_STATE_LEADER_DEGRADED,
                 "sleep 10s for debugging", term, index);
  });

  // prefetch stop
  consensus_log_manager.get_prefetch_manager()->disable_all_prefetch_channels();

  // rollback transactions not reached before_commit
  acquire_transaction_control_services();

  if (!opt_cluster_log_type_instance) {
    // Persist start apply index into consensus_info before killing all threads
    // and waiting all transactions finished. The transactions before it will be
    // committed. And the transactions after it will be rollbacked firstly and
    // replayed by applier thread if needed.
    Consensus_info *consensus_info = consensus_meta.get_consensus_info();
    consensus_info->set_start_apply_index(index);
    if (consensus_info->flush_info(true, true)) {
      error = 1;
      goto end;
    }

    // Update state change term after flushed consensus info
    current_state_degrade_term = term;
  }

  assert(this->status == Consensus_Log_System_Status::BINLOG_WORKING);

  DBUG_EXECUTE_IF("signal_before_downgrade_init_rli", {
    const char act[] = "now wait_for signal_downgrade_init_rli";
    assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });

  channel_map.rdlock();
  rli_info->mi->channel_wrlock();
  mysql_rwlock_wrlock(&LOCK_consensuslog_status);

  // Wait all active trx commit
  mysql_rwlock_wrlock(&LOCK_consensuslog_commit);
  mysql_rwlock_unlock(&LOCK_consensuslog_commit);

  // make sure that the transactions are committed in engines
  ha_flush_logs();

  // open relay log system
  mysql_mutex_lock(&rli_info->mi->data_lock);
  mysql_mutex_lock(&rli_info->data_lock);
  rli_info->cli_init_info();
  mysql_mutex_unlock(&rli_info->data_lock);
  mysql_mutex_unlock(&rli_info->mi->data_lock);

  // record new term
  current_term = term;
  this->status = Consensus_Log_System_Status::RELAY_LOG_WORKING;

  DBUG_EXECUTE_IF("signal_after_downgrade_init_rli", {
    const char act[] = "now signal after_downgrade_init_rli";
    assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });

  consensus_applier.set_stop_term(UINT64_MAX);

  if (!opt_cluster_log_type_instance) {
    // Save executed_gtids to tables and reset rli_info->gtid_set.
    if (gtid_state->save_gtids_of_last_binlog_into_table() ||
        (index < consensus_log_manager.get_sync_index()
             ? rli_info->reset_previous_gtid_set_of_consensus_log()
             : reset_previous_logged_gtids_relaylog(
                   rli_info->get_gtid_set(), rli_info->get_sid_lock()))) {
      mysql_rwlock_unlock(&LOCK_consensuslog_status);
      error = 1;
      goto end;
    }
    current_state_degrade_term = 0;

    error = start_consensus_apply_threads(rli_info->mi);
  }

  mysql_rwlock_unlock(&LOCK_consensuslog_status);

end:
  LogPluginErr(SYSTEM_LEVEL, ER_CONSENSUS_STATE_LEADER_DEGRADED,
               error ? "failed" : "finished", term, index);
  release_transaction_control_services();
  rli_info->mi->channel_unlock();
  channel_map.unlock();
  // recover prefetch
  consensus_log_manager.get_prefetch_manager()->enable_all_prefetch_channels();
  return error;
}

int ConsensusStateProcess::wait_follower_upgraded(uint64 term, uint64 index) {
  int error = 0;
  Consensus_info *consensus_info;
  DBUG_TRACE;

  LogPluginErr(SYSTEM_LEVEL, ER_CONSENSUS_STATE_FOLLOWER_UPGRADE, "started",
               term, index);

  assert(this->status == Consensus_Log_System_Status::RELAY_LOG_WORKING);

  // record new term
  // notice, the order of stop term and current term is important for apply
  // thread, because both stop term and current term is atomic variables. so do
  // not care reorder problem
  consensus_applier.set_stop_term(term);

  // wait replay thread to commit index
  if (!opt_cluster_log_type_instance) {
    DBUG_EXECUTE_IF("simulate_apply_too_slow", my_sleep(5000000););
    consensus_applier.wait_replay_log_finished();
    consensus_applier.wait_apply_threads_stop();
    if (!consensus_state_change_is_running) return 0;
  }

  // prefetch stop, and release LOCK_consensuslog_status
  consensus_log_manager.get_prefetch_manager()->disable_all_prefetch_channels();

  // kill all binlog dump thd
  binlog_dump_thread_kill();

  channel_map.rdlock();
  rli_info->mi->channel_wrlock();
  mysql_rwlock_wrlock(&LOCK_consensuslog_status);

  // Ensure all GTIDs to persist on disk.
  ha_flush_logs();

  mysql_mutex_lock(&rli_info->data_lock);
  rli_info->end_info();
  mysql_mutex_unlock(&rli_info->data_lock);

  mysql_mutex_t *log_lock = binlog->get_log_lock();
  mysql_mutex_lock(log_lock);
  binlog->lock_index();

  // close binlog system
  binlog->close(LOG_CLOSE_INDEX | LOG_CLOSE_TO_BE_OPENED, false, false);

  // open binlog index
  if (binlog->open_index_file(opt_binlog_index_name, opt_bin_logname, false)) {
    binlog->unlock_index();
    mysql_mutex_unlock(log_lock);
    mysql_rwlock_unlock(&LOCK_consensuslog_status);
    LogPluginErr(ERROR_LEVEL, ER_BINLOG_CANT_OPEN_FOR_LOGGING,
                 opt_binlog_index_name, errno);
    error = 1;
    goto end;
  }

  if (binlog->open_exist_consensus_binlog(opt_bin_logname, max_binlog_size,
                                          true, false)) {
    binlog->unlock_index();
    mysql_mutex_unlock(log_lock);
    mysql_rwlock_unlock(&LOCK_consensuslog_status);
    error = 2;
    goto end;
  }
  binlog->unlock_index();
  mysql_mutex_unlock(log_lock);
  this->status = Consensus_Log_System_Status::BINLOG_WORKING;

  // reset apply start point displayed in information_schema
  consensus_applier.set_apply_index(0);
  consensus_applier.set_real_apply_index(0);
  consensus_meta.clear_already_set_start();
  current_term = term;

  consensus_info = consensus_meta.get_consensus_info();
  // log type instance do not to recover start index
  if (!opt_cluster_log_type_instance) {
    consensus_info->set_last_leader_term(term);
  }
  consensus_info->set_recover_status(
      Consensus_Log_System_Status::BINLOG_WORKING);

  if (consensus_info->flush_info(true, true)) {
    mysql_rwlock_unlock(&LOCK_consensuslog_status);
    error = 3;
    goto end;
  }
  mysql_rwlock_unlock(&LOCK_consensuslog_status);
end:
  rli_info->mi->channel_unlock();
  channel_map.unlock();

  LogPluginErr(SYSTEM_LEVEL, ER_CONSENSUS_STATE_FOLLOWER_UPGRADE,
               error ? "failed" : "finished", term, index);

  consensus_log_manager.get_prefetch_manager()->enable_all_prefetch_channels();
  return error;
}

int ConsensusStateProcess::wait_follower_change_term(uint64 term) {
  DBUG_TRACE;
  LogPluginErr(SYSTEM_LEVEL, ER_CONSENSUS_STATE_FOLLOWER_CHANGE_TERM, term);
  current_term = term;
  return 0;
}

void ConsensusStateProcess::add_state_change_request(
    ConsensusStateChange &state_change) {
  DBUG_TRACE;
  mysql_mutex_lock(&LOCK_consensus_state_change);
  consensus_state_change_queue.push_back(state_change);
  mysql_cond_broadcast(&COND_consensus_state_change);
  mysql_mutex_unlock(&LOCK_consensus_state_change);
}

void ConsensusStateProcess::lock_consensus_state_change() {
  DBUG_TRACE;
  mysql_mutex_lock(&LOCK_consensus_state_change);
}

void ConsensusStateProcess::unlock_consensus_state_change() {
  DBUG_TRACE;
  mysql_mutex_unlock(&LOCK_consensus_state_change);
}

void ConsensusStateProcess::wait_state_change_cond() {
  DBUG_TRACE;
  mysql_cond_wait(&COND_consensus_state_change, &LOCK_consensus_state_change);
}

ConsensusStateChange ConsensusStateProcess::get_stage_change_from_queue() {
  ConsensusStateChange state_change;
  DBUG_TRACE;

  if (consensus_state_change_queue.size() > 0) {
    state_change = consensus_state_change_queue.front();
    consensus_state_change_queue.pop_front();
  }
  return state_change;
}

// #ifdef HAVE_REPLICATION
void *run_consensus_stage_change(void *arg) {
  THD *thd = nullptr;
  DBUG_TRACE;
  thd = new THD;
  thd->set_new_thread_id();

  if (my_thread_init()) return nullptr;

  thd->thread_stack = (char *)&thd;
  thd->store_globals();

  int error = 0;
  std::atomic<bool> *is_running = reinterpret_cast<std::atomic<bool> *>(arg);

  mysql_mutex_lock(&LOCK_server_started);
  while (!mysqld_server_started && (*is_running))
    mysql_cond_wait(&COND_server_started, &LOCK_server_started);
  mysql_mutex_unlock(&LOCK_server_started);

  while (*is_running) {
    consensus_state_process.lock_consensus_state_change();

    if (consensus_state_process.is_state_change_queue_empty() && (*is_running))
      consensus_state_process.wait_state_change_cond();
    if (consensus_state_process.is_state_change_queue_empty()) {
      consensus_state_process.unlock_consensus_state_change();
      continue;
    }

    ConsensusStateChange state_change =
        consensus_state_process.get_stage_change_from_queue();
    if (state_change.state != LEADER) {
      if (consensus_state_process.get_status() ==
          Consensus_Log_System_Status::BINLOG_WORKING) {
        error = consensus_state_process.wait_leader_degraded(
            state_change.term, state_change.index);
      } else if (state_change.state != CANDIDATE) {
        error = consensus_state_process.wait_follower_change_term(
            state_change.term);
      }
    } else {
      // must be candidate ->leader
      error = consensus_state_process.wait_follower_upgraded(
          state_change.term, state_change.index);
    }

    consensus_state_process.unlock_consensus_state_change();

    if (error) abort();
  }

  thd->release_resources();
  delete thd;
  my_thread_end();
  return nullptr;
}
// #endif

bool ConsensusStateProcess::is_state_machine_ready() {
  assert(rpl_consensus_get_term() >= get_current_term());
  return status == Consensus_Log_System_Status::BINLOG_WORKING &&
         rpl_consensus_get_term() == get_current_term();
}

int ConsensusStateProcess::start_consensus_state_change_thread() {
  DBUG_TRACE;
  consensus_state_change_is_running = true;
  if (mysql_thread_create(key_thread_consensus_stage_change,
                          &consensus_state_change_thread_handle, nullptr,
                          run_consensus_stage_change,
                          (void *)&consensus_state_change_is_running)) {
    consensus_state_change_is_running = false;
    LogPluginErr(ERROR_LEVEL, ER_CONSENSUS_CREATE_THRERD_ERROR,
                 "run_consensus_stage_change");
    abort();
  }
  return 0;
}

int ConsensusStateProcess::init_service() {
  DBUG_TRACE;
  Consensus_info *consensus_info = consensus_meta.get_consensus_info();

  // learner's cluster_info is empty or not contain @
  bool is_learner =
      consensus_info->get_cluster_info() == "" ||
      consensus_info->get_cluster_info().find('@') == std::string::npos;

  uint64 mock_start_index =
      max(consensus_log_manager.get_log_file_index()->get_first_index(),
          (uint64)opt_consensus_start_index);
  rpl_consensus_init(is_learner, mock_start_index, &consensus_log_manager,
                     &consensus_meta, this);

  return 0;
}

int ConsensusStateProcess::recovery_applier_status() {
  uint64 next_index = 0;
  my_off_t log_pos = 0;
  char log_name[FN_REFLEN];
  DBUG_TRACE;

  // Load mts recovery end index for snapshot recovery
  if (!opt_cluster_log_type_instance &&
      (consistent_snapshot_recovery || opt_cluster_recover_from_snapshot) &&
      mts_recovery_max_consensus_index()) {
    return -1;
  }

  set_status(Consensus_Log_System_Status::BINLOG_WORKING);

  // Get applier start index from consensus_applier_info
  if (!opt_initialize && !opt_cluster_log_type_instance)
    next_index = get_applier_start_index();

  if (!opt_initialize && !opt_cluster_log_type_instance &&
      !consensus_log_manager.get_start_without_log()) {
    assert(next_index > 0);
    if (consensus_log_manager.get_log_position(next_index, false, log_name,
                                               &log_pos)) {
      LogPluginErr(ERROR_LEVEL, ER_CONSENSUS_LOG_FIND_POSITION_ERROR,
                   next_index, "recovery applier status");
      abort();
    }
  }

  // Reached this, the applier start index of consensus is set
  if (gtid_init_after_consensus_setup(
          next_index, (!consensus_log_manager.get_start_without_log() &&
                       next_index > 0 && consistent_snapshot_recovery)
                          ? log_name
                          : nullptr)) {
    return -1;
  }

  if (!opt_initialize && !opt_cluster_log_type_instance &&
      consensus_log_manager.get_start_without_log()) {
    assert(next_index > 0);
    if (consensus_log_manager.get_log_position(next_index, false, log_name,
                                               &log_pos)) {
      LogPluginErr(ERROR_LEVEL, ER_CONSENSUS_LOG_FIND_POSITION_ERROR,
                   next_index, "recovery applier status");
      abort();
    }
  }

  if (!opt_initialize && !opt_cluster_log_type_instance &&
      (opt_cluster_recover_from_snapshot || opt_cluster_recover_from_backup)) {
    if (opt_cluster_archive_recovery && opt_archive_log_index_name != nullptr) {
      Consensus_info *consensus_info = consensus_meta.get_consensus_info();
      uint64 recover_status = consensus_info->get_recover_status();

      if (recover_status != Consensus_Log_System_Status::RELAY_LOG_WORKING ||
          consensus_info->get_start_apply_index() == 0) {
        if (next_index > 0) consensus_log_manager.truncate_log(next_index);

        if (next_index > 0 && consensus_info->get_start_apply_index() == 0)
          consensus_info->set_start_apply_index(next_index - 1);

        if (recover_status != Consensus_Log_System_Status::RELAY_LOG_WORKING)
          consensus_info->set_recover_status(
              Consensus_Log_System_Status::RELAY_LOG_WORKING);

        consensus_info->flush_info(true, true);
      }

      /* Generate new binlog files from archive logs */
      if (consensus_open_archive_log(
              consensus_log_manager.get_log_file_index()->get_first_index(),
              consensus_log_manager.get_sync_index()))
        return -1;

      /* Advance term to last log term */
      if (get_current_term() > consensus_info->get_current_term()) {
        consensus_info->set_current_term(get_current_term());
        consensus_info->flush_info(true, true);
        LogPluginErr(SYSTEM_LEVEL, ER_CONSENSUS_ARCHIVE_RECOVERY_ADVANCE_TERM,
                     get_current_term());
      }
    } else if (next_index > 0) {
      consensus_log_manager.truncate_log(next_index);
    }
  }

  if (!opt_initialize) {
    // Recovery finished, start consensus service from follower
    set_status(Consensus_Log_System_Status::RELAY_LOG_WORKING);

    if (!opt_cluster_log_type_instance) {
      mysql_mutex_lock(binlog->get_log_lock());
      binlog->switch_and_seek_log(log_name, log_pos, true);
      mysql_mutex_unlock(binlog->get_log_lock());
    }

    // Set right current term for apply thread
    set_current_term(consensus_meta.get_consensus_info()->get_current_term());
  }

  return 0;
}

int ConsensusStateProcess::stop_consensus_state_change_thread() {
  DBUG_TRACE;
  if (inited && consensus_state_change_is_running) {
    consensus_state_change_is_running = false;
    mysql_mutex_lock(&LOCK_consensus_state_change);
    mysql_cond_broadcast(&COND_consensus_state_change);
    mysql_cond_broadcast(&COND_server_started);
    mysql_mutex_unlock(&LOCK_consensus_state_change);
    my_thread_join(&consensus_state_change_thread_handle, nullptr);
  }
  return 0;
}

int ConsensusStateProcess::cleanup() {
  DBUG_TRACE;
  if (inited) {
    mysql_rwlock_destroy(&LOCK_consensuslog_status);
    mysql_rwlock_destroy(&LOCK_consensuslog_commit);
    mysql_mutex_destroy(&LOCK_consensuslog_term);
    mysql_mutex_destroy(&LOCK_consensus_state_change);
    mysql_cond_destroy(&COND_consensus_state_change);
  }
  return 0;
}