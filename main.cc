// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstdint>
#include <fstream>
#include <future>  // NOLINT
#include <iostream>
#include <string>
#include <vector>

#include "third_party/absl/flags/flag.h"
#include "third_party/absl/flags/parse.h"
#include "third_party/absl/time/clock.h"
#include "third_party/absl/time/time.h"
#include "engine/help_database.h"
#include "engine/log_writer.h"
#include "engine/report_engine.h"
#include "engine/sampling_engine.h"
#include "providers/cgroup_provider.h"
#include "providers/metric_snapshot.h"
#include "providers/numa_provider.h"
#include "providers/proc_provider.h"

ABSL_FLAG(absl::Duration, memory_duration, absl::Minutes(5),
          "Duration to record memory metrics");
ABSL_FLAG(absl::Duration, sample, absl::Seconds(5), "Sampling interval");
ABSL_FLAG(std::string, output, "", "Destination path for the output log file");
ABSL_FLAG(std::string, start, "", "Start timestamp for report");
ABSL_FLAG(std::string, end, "", "End timestamp for report");
ABSL_FLAG(std::string, input, "", "Input log file for report");

int main(int argc, char* argv[]) {
  // Parse command line flags
  std::vector<char*> positional = absl::ParseCommandLine(argc, argv);

  if (positional.size() < 2) {
    std::cerr << "Usage: " << argv[0] << " <record|report|explain|erase> [args...]\n";
    return 1;
  }

  std::string mode = positional[1];

  if (mode == "erase") {
    std::string output_path = absl::GetFlag(FLAGS_output);
    if (output_path.empty()) {
      std::cerr << "Erase mode requires --output flag.\n";
      return 6;
    }
    std::ofstream out(output_path, std::ios::trunc);
    if (out.fail()) {
      std::cerr << "Failed to erase log file: " << output_path << "\n";
      return 7;
    }
    std::cout << "Erased log file: " << output_path << "\n";
  } else if (mode == "record") {
    std::cout << "Kernel Metrics Agent Started in Record Mode!" << std::endl;
    std::cout << "Recording duration: " << absl::GetFlag(FLAGS_memory_duration)
              << std::endl;
    std::cout << "Sampling interval: " << absl::GetFlag(FLAGS_sample)
              << std::endl;

    std::string output_path = absl::GetFlag(FLAGS_output);
    guest_memory_metrics::LogWriter log_writer(output_path);
    auto status = log_writer.Open();
    if (!status.ok()) {
      std::cerr << "Failed to open output log file: " << status.message()
                << std::endl;
      return 2;
    }

    guest_memory_metrics::ProcProvider proc_provider;
    guest_memory_metrics::CgroupProvider cgroup_provider;
    guest_memory_metrics::NumaProvider numa_provider;

    // Define the callback that will be executed on every tick
    auto sample_callback = [&log_writer, &proc_provider, &cgroup_provider,
                            &numa_provider]() {
      int64_t current_ts = absl::ToUnixMillis(absl::Now());
      std::cout << "Collecting sample at " << current_ts << "..." << std::endl;

      guest_memory_metrics::MetricSnapshot proc_snapshot =
          proc_provider.GetSnapshot();
      guest_memory_metrics::MetricSnapshot cgroup_snapshot =
          cgroup_provider.GetSnapshot();
      guest_memory_metrics::MetricSnapshot numa_snapshot =
          numa_provider.GetSnapshot();

      for (const auto& [metric_name, value] : proc_snapshot.metrics) {
        log_writer.WriteMetric(current_ts, "host", metric_name, value);
      }
      for (const auto& [metric_name, value] : cgroup_snapshot.metrics) {
        log_writer.WriteMetric(current_ts, "cgroup", metric_name, value);
      }
      for (const auto& [metric_name, value] : numa_snapshot.metrics) {
        log_writer.WriteMetric(current_ts, "numa", metric_name, value);
      }
    };

    guest_memory_metrics::SamplingEngine engine(
        absl::GetFlag(FLAGS_sample), absl::GetFlag(FLAGS_memory_duration),
        sample_callback);

    engine.Start();
    absl::SleepFor(absl::GetFlag(FLAGS_memory_duration) + absl::Seconds(1));
    engine.Stop();
    log_writer.Close();

    std::cout << "Agent shutting down." << std::endl;

  } else if (mode == "report") {
    std::string input_path = absl::GetFlag(FLAGS_input);
    std::string start_str = absl::GetFlag(FLAGS_start);
    std::string end_str = absl::GetFlag(FLAGS_end);

    if (input_path.empty() || start_str.empty() || end_str.empty()) {
      std::cerr << "Report mode requires --input, --start, and --end flags.\n";
      return 3;
    }

    guest_memory_metrics::ReportEngine engine;
    engine.GenerateReport(input_path, start_str, end_str);

  } else if (mode == "explain") {
    if (positional.size() < 3) {
      std::cerr << "Usage: " << argv[0] << " explain <metric_name>\n";
      return 4;
    }
    std::string metric_name = positional[2];
    std::cout << guest_memory_metrics::GetHelpForMetric(metric_name) << "\n";
  } else {
    std::cerr << "Unknown mode: " << mode << "\n";
    return 5;
  }

  return 0;
}
