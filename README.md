# Guest Memory Metrics Agent

**Note:** This project is currently under development.

## Overview

The **Guest Memory Metrics Agent** (also known as the Kernel Metrics Collector)
is a lightweight, open-source guest-resident daemon and command-line interface
(CLI) tool written in C++ for Linux systems.

### Project Goals
Analyzing guest-level memory management issues inside virtual machines often
presents a significant challenge due to a lack of granular, low-overhead
historical insights into guest kernel subsystems. The primary goal of this
project is to seamlessly collect, timestamp, store, and analyze kernel metrics
across cgroup and memory subsystems with minimal performance overhead. This
enables rapid triage of memory pressure, page fault latency, and unexpected
resource constraints, significantly accelerating the time-to-resolution for
kernel-level anomalies.

### What It Does
The agent operates via a single binary with a unified workflow, supporting four
distinct modes:

* **Record Mode (`record`):** Runs as a low-overhead background daemon,
  periodically sampling system virtual filesystems (`/proc`, `/sys/fs/cgroup`,
  `/sys/devices/system/node`). It safely scrubs Personally Identifiable
  Information (PII)—such as container hashes and specific user paths—before
  saving the snapshots to a structured local log file.
* **Report Mode (`report`):** Acts as an offline reporting engine. It parses
  the recorded logs, computes metric deltas over a user-specified time window
  (e.g., matching the exact time an incident occurred), and outputs a
  comparative analysis to instantly identify spikes in specific memory counters
  (like `pglazyfreed` or `pgfault`).
* **Explain Mode (`explain`):** Queries a localized internal database of kernel
  metrics to explain their definitions, units, and potential impacts,
  empowering users with immediate, actionable context.
* **Erase Mode (`erase`):** Truncates/clears the contents of existing log files specified by the
  `--output` flag. This allows customers to easily clear out historical memory
  snapshots and free up disk space on their VMs when the logs are no longer
  needed.

### Intended Users
This tool is expressly built for:

* **Google Compute Engine (GCE) Customers:** To run proactively or reactively
  within their VMs for self-service diagnosis and localized troubleshooting
  without needing deep kernel expertise.
* **Google Cloud Support & Engineering Teams:** To securely and standardizedly
  gather PII-scrubbed diagnostic data during support escalations, eliminating
  the reliance on custom, ad-hoc shell scripts.

## Build and Usage Instructions

### 1. Building the Agent
To ensure the binary runs correctly on standard Linux environments (without
relying on custom dynamic linkers), it must be built statically.

**For External / Open-Source Users (using Bazel):**
```bash
bazel build --features=fully_static_link //third_party/guest_memory_metrics_agent:kernel_metrics_agent
```

**For Internal Google Engineers (using Blaze):**
```bash
SKYBUILD=1 blaze build --features=fully_static_link //third_party/guest_memory_metrics_agent:kernel_metrics_agent
```

### 2. Deploying to a VM
Transfer the compiled binary and the systemd service file to your target VM
using `gcloud compute scp`. Ensure you replace `<INSTANCE_NAME>`, `<PROJECT_ID>`,
and `<ZONE>` with your specific VM details.

*(Note: Internal Google users should substitute `bazel-bin` with `blaze-bin` in the path below.)*

```bash
gcloud compute scp bazel-bin/third_party/guest_memory_metrics_agent/kernel_metrics_agent <INSTANCE_NAME>:~/ --project=<PROJECT_ID> --zone=<ZONE>
gcloud compute scp third_party/guest_memory_metrics_agent/guest-memory-metrics-agent.service <INSTANCE_NAME>:~/ --project=<PROJECT_ID> --zone=<ZONE>
```

### 3. Installation
SSH into your VM and move the deployed files to their standard system locations.

```bash
# SSH into the VM
gcloud compute ssh <INSTANCE_NAME> --project=<PROJECT_ID> --zone=<ZONE>

# 1. Install the binary
sudo mv ~/kernel_metrics_agent /usr/local/bin/
sudo chmod +x /usr/local/bin/kernel_metrics_agent

# 2. Install the systemd service file
sudo mv ~/guest-memory-metrics-agent.service /etc/systemd/system/

# 3. Reload systemd so it recognizes the new service
sudo systemctl daemon-reload
```

### 4. Automated Startup and Background Execution (Recommended)
For continuous, secure monitoring with least-privilege (granting
`CAP_DAC_READ_SEARCH` without running as full root), use the provided `systemd`
service. This ensures the agent runs autonomously and restarts on boot.

**To enable the service to start automatically on system boot:**
```bash
sudo systemctl enable guest-memory-metrics-agent.service
```

**To start the service immediately:**
```bash
sudo systemctl start guest-memory-metrics-agent.service
```
*(Tip: You can do both at once with `sudo systemctl enable --now guest-memory-metrics-agent.service`)*

**To verify the agent is running correctly:**
```bash
sudo systemctl status guest-memory-metrics-agent.service
```
*(Look for `Active: active (running)` to confirm it is collecting data).*

**To stop the service:**
```bash
sudo systemctl stop guest-memory-metrics-agent.service
```

**To view live real-time logs:**
```bash
sudo journalctl -u guest-memory-metrics-agent.service -f
```

### 5. Generating Reports from the Daemon
The background service automatically logs metrics to
`/var/log/guest-memory-metrics-agent/metrics.log`. To generate a comparative
analysis report from these logs:

1. Identify the specific time window you want to analyze. Find the exact start
   and stop timestamps recorded by the daemon:
   ```bash
   sudo journalctl -u guest-memory-metrics-agent.service | grep "AGENT_"
   ```
2. Run the report command using the daemon's log file and your chosen
   timestamps:
   ```bash
   sudo kernel_metrics_agent report --input=/var/log/guest-memory-metrics-agent/metrics.log --start=<START_TIMESTAMP> --end=<STOP_TIMESTAMP>
   ```

### 6. Running Manually in the Foreground
If you prefer not to use the background daemon, you can run the agent
interactively to collect a quick sample and immediately generate a report:

```bash
# Record for 60 seconds, sampling every 2 seconds
sudo kernel_metrics_agent record --output=/tmp/metrics.log --memory_duration=60s --sample=2s

# Generate a comparative report from the recorded data
sudo kernel_metrics_agent report --input=/tmp/metrics.log --start=0 --end=3000000000000
```

### 7. Explain Mode
If you encounter a metric in your report that you do not understand, query the
agent's built-in help database for an immediate explanation of what the metric
means and how it impacts system memory:

```bash
kernel_metrics_agent explain pgmajfault
```

### 8. Erase Mode
If you need to free up disk space and want to safely truncate/clear an existing metrics
log file, you can use the erase mode.

**To erase a manual foreground log:**
```bash
sudo kernel_metrics_agent erase --output=/tmp/metrics.log
```

**To erase the background daemon's log:**
First, check if the agent is currently running:
```bash
sudo systemctl status guest-memory-metrics-agent.service
```
If it is running, make sure you stop the service first to prevent the daemon
from continuing to append logs into the truncated file at unpredictable byte
offsets.
```bash
sudo systemctl stop guest-memory-metrics-agent.service
sudo kernel_metrics_agent erase --output=/var/log/guest-memory-metrics-agent/metrics.log
```

*(You can then run `sudo systemctl start guest-memory-metrics-agent.service`
whenever you are ready to begin recording metrics again.)*

## Deployment and Terms

This tool is designed to be executed within [Google Compute Engine (GCE)
Virtual Machines](https://cloud.google.com/compute). By utilizing this software
within your Google Cloud environment, your usage is subject to the [Google Cloud
Platform Terms of Service](https://cloud.google.com/terms).

---

## Disclaimers

This is not an officially supported Google product. This project is not eligible
for the [Google Open Source Software Vulnerability Rewards
Program](https://bughunters.google.com/open-source-security).
