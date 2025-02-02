//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.
//
#pragma once

#include "env/env.h"
#include "monitoring/perf_level_imp.h"

namespace smartengine {
namespace monitor {

class PerfStepTimer {
 public:
  explicit PerfStepTimer(uint64_t* metric, bool for_mutex = false)
      : enabled_(
            perf_level >= PerfLevel::kEnableTime ||
            (!for_mutex && perf_level >= PerfLevel::kEnableTimeExceptForMutex)),
        env_(enabled_ ? util::Env::Default() : nullptr),
        start_(0),
        metric_(metric) {}

  ~PerfStepTimer() { Stop(); }

  void Start() {
    if (enabled_) {
      start_ = env_->NowNanos();
    }
  }

  void Measure() {
    if (start_) {
      uint64_t now = env_->NowNanos();
      *metric_ += now - start_;
      start_ = now;
    }
  }

  void Stop() {
    if (start_) {
      *metric_ += env_->NowNanos() - start_;
      start_ = 0;
    }
  }

 private:
  const bool enabled_;
  util::Env* const env_;
  uint64_t start_;
  uint64_t* metric_;
};

}  // namespace monitor
}  // namespace smartengine
