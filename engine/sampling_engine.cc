#include "third_party/guest_memory_metrics_agent/engine/sampling_engine.h"

#include <chrono>  // NOLINT
#include <errno.h>
#include <signal.h>
#include <time.h>

#include <functional>
#include <iostream>
#include <utility>

#include "third_party/absl/time/time.h"

namespace guest_memory_metrics {

SamplingEngine::SamplingEngine(absl::Duration interval, absl::Duration duration,
                               std::function<void()> callback)
    : interval_(interval),
      duration_(duration),
      callback_(std::move(callback)),
      running_(false) {}

SamplingEngine::~SamplingEngine() { Stop(); }

void SamplingEngine::Start() {
  running_ = true;

  // Block SIGALRM before creating the thread so the main thread and
  // the new thread both have it blocked. This prevents the OS from
  // delivering the signal to the main thread and terminating the process.
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGALRM);
  pthread_sigmask(SIG_BLOCK, &sigset, nullptr);

  thread_ = std::thread(&SamplingEngine::Loop, this);
}

void SamplingEngine::Stop() {
  if (running_.exchange(false)) {
    if (thread_.joinable()) {
      thread_.join();
    }
  }
}

void SamplingEngine::Loop() {
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGALRM);

  // Block SIGALRM in this thread so sigwait/sigtimedwait can catch it
  pthread_sigmask(SIG_BLOCK, &sigset, nullptr);

  struct sigevent sev;
  timer_t timerid;

  sev.sigev_notify = SIGEV_SIGNAL;
  sev.sigev_signo = SIGALRM;
  sev.sigev_value.sival_ptr = &timerid;

  if (timer_create(CLOCK_MONOTONIC, &sev, &timerid) == -1) {
    std::cerr << "Failed to create timer" << std::endl;
    return;
  }

  struct itimerspec its;
  its.it_value = absl::ToTimespec(interval_);
  its.it_interval = absl::ToTimespec(interval_);

  if (timer_settime(timerid, 0, &its, nullptr) == -1) {
    std::cerr << "Failed to set timer" << std::endl;
    timer_delete(timerid);
    return;
  }

  auto start_time = std::chrono::steady_clock::now();
  auto duration_ns =
      std::chrono::nanoseconds(absl::ToInt64Nanoseconds(duration_));
  auto end_time = start_time + duration_ns;

  while (running_ && std::chrono::steady_clock::now() < end_time) {
    auto now = std::chrono::steady_clock::now();
    absl::Duration wait_duration = interval_;
    if (wait_duration <= absl::ZeroDuration()) {
      wait_duration = absl::Seconds(1);
    }
    auto remaining_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - now);
    if (remaining_ns.count() > 0) {
      absl::Duration remaining_duration =
          absl::Nanoseconds(remaining_ns.count());
      if (wait_duration > remaining_duration) {
        wait_duration = remaining_duration;
      }
    }
    struct timespec timeout = absl::ToTimespec(wait_duration);
    if (timeout.tv_sec <= 0 && timeout.tv_nsec <= 0) {
      timeout.tv_sec = 0;
      timeout.tv_nsec = 1000000;
    }
    int ret = sigtimedwait(&sigset, nullptr, &timeout);

    if (ret == SIGALRM) {
      callback_();
    } else if (ret == -1 && errno == EAGAIN) {
      // Timeout, loop back and check conditions
      continue;
    }
  }

  timer_delete(timerid);
}

}  // namespace guest_memory_metrics
