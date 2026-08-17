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

### How the PII Scrubber Works
The embedded PII scrubber continuously anonymizes cgroups, procfs, and system memory metrics across Linux guest VMs and container environments to prevent the leakage of tenant topology. This is specifically needed as raw metric paths frequently contain user directories, user IDs, container IDs, Kubernetes pod names, and custom user cgroup names.

<details>
<summary><b>Technical Data: PII Scrubber Internals</b></summary>

#### Concrete Transformation Examples

| Raw Input Path | Scrubbed Output Path | Explanation / Behavior |
| :---: | :---: | :---: |
| `proc/meminfo/MemFree` | `proc/meminfo/MemFree` | Allowed: Known kernel metric tokens in `kAllowList`. |
| `cgroup/cpu0/usage_in_bytes` | `cgroup/cpu0/usage_in_bytes` | Allowed: Metric prefix `cpu` + trailing digits `0` permitted. |
| `sys/node1/memory.stat` | `sys/node1/memory.stat` | Allowed: Metric prefix `node` + trailing digits `1` permitted. |
| `net/eth0/stat` | `net/eth0/stat` | Allowed: Metric prefix `eth` + trailing digits `0` permitted. |
| `disk/nvme0n1/stat` | `disk/nvme0n1/stat` | Allowed: NVMe drive naming scheme (`nvme` + `0` + `n` + `1`). |
| `block/sda1/stat` | `block/sda1/stat` | Allowed: SCSI/VirtIO block device scheme (`sd` + `a` + `1`). |
| `/home/johndoe/proc/meminfo` | `/home/[REDACTED]/proc/meminfo` | Redacted: User directory `/home/johndoe` redacted; trailing metric `proc/meminfo` preserved. |
| `/users/alice/` | `/users/[REDACTED]` | Redacted: User directory `/users/alice` redacted without trailing metric. |
| `cgroup/pod12345/memory.stat` | `cgroup/pod[HASH:9f8e7d6c5b4a3210]` | Hashed: Structural prefix `pod` + digits `12345` hashed to prevent numeric ID bypass. |
| `sys/user1000/cpu.stat` | `sys/user[HASH:1a2b3c4d5e6f7a8b]` | Hashed: Structural prefix `user` + digits `1000` hashed to prevent numeric UID bypass. |
| `cgroup/Bob_App/memory.current` | `cgroup/[HASH:4f3e2d1c0b9a8f7e]` | Hashed: Custom application cgroup `Bob_App` defaults to salted FNV-1a hash. |
| `docker-a1b2c3d4e5f6` | `docker-[HASH:8c7b6a5f4e3d2c1b]` | Hashed: Container ID prefix `docker-` preserved; random hash output. |

#### Implementation Flowchart

| 1. Input & Tokenization |
| :--- |
| **[Input]** Raw Metric Path Input<br/>↓<br/>**[Step 1]** User Directory Scan (`/home/` & `/users/`) ➔ Redact user subdirectories, preserve trailing metric keywords<br/>↓<br/>**[Step 2]** O(N) Tokenizer ➔ Split string by `/` and `.` |
| **2. Token Decision Tree** |
| *Decision 1: Is Token in `kAllowList`?*<br/>YES ➔ [Keep Verbatim] (e.g., `cgroup`, `proc`) |
| *Decision 2: Digits Check — Does prefix match pure metric? (`cpu`, `node`, `eth`, `sd`, `nvme`)*<br/>YES ➔ [Keep Verbatim] (e.g., `cpu0`, `sda1`, `nvme0n1`) |
| *Decision 3: Matches Contextual Prefix? (`pod`, `user-`, `session-`, `docker-`, etc.)*<br/>YES ➔ Emit prefix + `[HASH: SaltedFNV1a(suffix)]`<br/>NO ➔ Emit `[HASH: SaltedFNV1a(token)]` |
| **3. Cache & Output** |
| Reconstruct Delimiters & Write to Mutex-Guarded SwissTable Cache<br/>↓<br/>Output Strict RFC 8259 JSON Escaped Line |

#### 1. Machine-Salted FNV-1a Hash

The scrubber seeds its hash using `/etc/machine-id`. This prevents cross-machine rainbow table attacks while preserving time-series metric cardinality on the same host across agent restarts.

```cpp
static const uint64_t kHashSalt = []() -> uint64_t {
    std::ifstream f("/etc/machine-id");
    std::string id;
    if (f >> id && !id.empty()) {
        uint64_t hash = 0xcbf29ce484222325ULL;
        for (char c : id) {
            hash ^= static_cast<uint64_t>(static_cast<uint8_t>(c));
            hash *= 0x100000001b3ULL;
        }
        return hash;
    }
    return 0xcbf29ce484222325ULL;
}();
```

#### 2. Restricting Digit Suffixes to Physical Metrics

Trailing digits are permitted only if the prefix belongs to `kMetricAllowList` (`cpu`, `node`, `eth`, `nvme`, `sd`, `vd`). Structural folders like `pod1234` or `user1000` fail this test and are hashed.

```cpp
auto is_allowed_token =
    [&](const std::string& lower_tok,
        const absl::flat_hash_set<std::string>& allow_set) -> bool {
    if (allow_set.contains(lower_tok)) return true;
    size_t first_digit = lower_tok.find_first_of("0123456789");
    if (first_digit != std::string::npos && first_digit > 0) {
        std::string prefix = lower_tok.substr(0, first_digit);
        // Only allow dynamic trailing digits for pure metric endpoints
        if (kMetricAllowList->contains(prefix)) {
            std::string suffix = lower_tok.substr(first_digit);
            if (prefix == "nvme") {
                // Validate nvme0n1 format
                ...
            }
            bool all_digits = true;
            for (char d : suffix) {
                if (d < '0' || d > '9') { all_digits = false; break; }
            }
            if (all_digits) return true;
        }
    }
    return false;
};
```
</details>

### Intended Users
This tool is expressly built for:

* **Google Compute Engine (GCE) Customers:** To run proactively or reactively
  within their VMs for self-service diagnosis and localized troubleshooting
  without needing deep kernel expertise.
* **Google Cloud Support & Engineering Teams:** To securely and standardizedly
  gather PII-scrubbed diagnostic data during support escalations, eliminating
  the reliance on custom, ad-hoc shell scripts.

## Installation and Usage Instructions

### 1. Fast Binary Installation (Recommended)
Pre-compiled, fully statically linked standalone binaries are available for each release across both `x86_64` (AMD64) and `aarch64` (ARM64) architectures. No compilers or external dependencies are required.

Run the following commands directly on your Linux VM to install the agent:

```bash
# 1. Detect architecture (x86_64 -> amd64, aarch64 -> arm64)
ARCH=$(uname -m | sed 's/x86_64/amd64/;s/aarch64/arm64/')
case "${ARCH}" in
  amd64|arm64) ;;
  *) echo "Unsupported architecture: $(uname -m)" >&2; exit 1 ;;
esac
VERSION="latest" # Set to "latest" or a specific release tag, e.g. "v1.0.0"

# 2. Construct download URL based on version
if [ "${VERSION}" = "latest" ]; then
  DOWNLOAD_URL="https://github.com/GoogleCloudPlatform/guest-memory-metrics-agent/releases/latest/download"
else
  DOWNLOAD_URL="https://github.com/GoogleCloudPlatform/guest-memory-metrics-agent/releases/download/${VERSION}"
fi

# 3. Download pre-compiled static binary
sudo curl -fsSL \
  "${DOWNLOAD_URL}/kernel_metrics_agent-linux-${ARCH}" \
  -o /usr/local/bin/kernel_metrics_agent
sudo chmod +x /usr/local/bin/kernel_metrics_agent

# 4. Download systemd service file
sudo curl -fsSL \
  "${DOWNLOAD_URL}/guest-memory-metrics-agent.service" \
  -o /etc/systemd/system/guest-memory-metrics-agent.service

# 5. Reload systemd daemon
sudo systemctl daemon-reload
```

### 2. Building from Source (Optional)
If you prefer to compile from source using Bazel, build statically using `--features=fully_static_link`:

```bash
# 1. Clone repository
git clone https://github.com/GoogleCloudPlatform/guest-memory-metrics-agent.git
cd guest-memory-metrics-agent

# 2. Build static binary
bazel build -c opt --features=fully_static_link //:kernel_metrics_agent

# 3. Install binary and service
sudo cp bazel-bin/kernel_metrics_agent /usr/local/bin/
sudo chmod +x /usr/local/bin/kernel_metrics_agent
sudo cp guest-memory-metrics-agent.service /etc/systemd/system/
sudo systemctl daemon-reload
```

### 3. Automated Startup and Background Execution (Recommended)
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

### 4. Generating Reports from the Daemon
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

### 5. Running Manually in the Foreground
If you prefer not to use the background daemon, you can run the agent
interactively to collect a quick sample and immediately generate a report:

```bash
# Record for 60 seconds, sampling every 2 seconds
sudo kernel_metrics_agent record --output=/tmp/metrics.log --memory_duration=60s --sample=2s

# Generate a comparative report from the recorded data
sudo kernel_metrics_agent report --input=/tmp/metrics.log --start=0 --end=3000000000000
```

### 6. Explain Mode
If you encounter a metric in your report that you do not understand, query the
agent's built-in help database for an immediate explanation of what the metric
means and how it impacts system memory:

```bash
kernel_metrics_agent explain pgmajfault
```

### 7. Erase Mode
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

### 8. Uninstallation

Because the agent is a single, statically-linked binary with zero external dependencies, uninstallation is incredibly fast and leaves no trace on the VM.

**Scenario 1: Uninstalling the Background Daemon (systemd)**
```bash
# 1. Stop and disable the background service
sudo systemctl disable --now guest-memory-metrics-agent.service

# 2. (Optional) Safely erase telemetry history
sudo kernel_metrics_agent erase --output=/var/log/guest-memory-metrics-agent/metrics.log

# 3. Delete the configuration files, logs, and binary
sudo rm -f /etc/systemd/system/guest-memory-metrics-agent.service
sudo rm -f /usr/local/bin/kernel_metrics_agent
sudo rm -rf /var/log/guest-memory-metrics-agent/

# 4. Reload systemd state
sudo systemctl daemon-reload
```

**Scenario 2: Uninstalling an Ad-Hoc / Foreground Run**
```bash
# Safely clear the logs
sudo /tmp/kernel_metrics_agent erase --output=/tmp/metrics.log

# Delete the downloaded binary and log file
rm -f /tmp/kernel_metrics_agent /tmp/metrics.log
```

### 9. Log Rotation Configuration
If you run the background daemon continuously, the log file at `/var/log/guest-memory-metrics-agent/metrics.log` will grow over time. The agent does not deploy its own log rotation rules by default.

If you choose to manage this file using the standard Linux `logrotate` utility, you **must include the `copytruncate` directive** in your configuration. Because the agent keeps the log file's descriptor open while it is running, standard rotation (which renames the file) will cause the agent to continue writing to the rotated, old file instead of the new one. The `copytruncate` directive safely truncates the original file in place, seamlessly working with the running agent. Because the agent uses systemd `DynamicUser`, `copytruncate` is also strictly required to prevent logrotate from creating new files with incorrect `root` permissions, which will break the agent.

Here is an example configuration you can place in `/etc/logrotate.d/guest-memory-metrics-agent`:

```text
/var/log/guest-memory-metrics-agent/metrics.log {
    daily
    size 100M
    rotate 7
    missingok
    notifempty
    copytruncate
}
```
### 10. Troubleshooting & Support

If you encounter an issue where the `guest-memory-metrics-agent.service` fails to start, crashes unexpectedly, or exhibits other unusual behavior, you can extract the daemon's internal error logs to share with Google Cloud Support.

Because the daemon strictly separates operational health information (which is routed to `stderr`) from actual memory snapshots, your internal error logs are safely isolated in the system journal.

To export the last 3 days of operational logs to a single text file that you can attach to your Support ticket, run:

```bash
sudo journalctl -u guest-memory-metrics-agent.service --since "3 days ago" --no-pager > guest_metrics_agent_debug.log
```

*Note: This log file exclusively tracks the health, configuration, and initialization steps of the agent. It does not contain the historical memory metric snapshots from your virtual machine.*

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
