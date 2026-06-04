# Guest Memory Metrics Agent

**Note:** This project is currently under development.

## Overview

The **Guest Memory Metrics Agent** (also known as the Kernel Metrics Collector) is a lightweight, open-source guest-resident daemon and command-line interface (CLI) tool written in C++ for Linux systems. 

### Project Goals
Analyzing guest-level memory management issues inside virtual machines often presents a significant challenge due to a lack of granular, low-overhead historical insights into guest kernel subsystems. The primary goal of this project is to seamlessly collect, timestamp, store, and analyze kernel metrics across cgroup and memory subsystems with minimal performance overhead. This enables rapid triage of memory pressure, page fault latency, and unexpected resource constraints, significantly accelerating the time-to-resolution for kernel-level anomalies.

### What It Does
The agent operates via a single binary with a unified workflow, supporting three distinct modes:
* **Record Mode (`-record`):** Runs as a low-overhead background daemon, periodically sampling system virtual filesystems (`/proc`, `/sys/fs/cgroup`, `/sys/devices/system/node`). It safely scrubs Personally Identifiable Information (PII)—such as container hashes and specific user paths—before saving the snapshots to a structured local log file.
* **Report Mode (`-report`):** Acts as an offline reporting engine. It parses the recorded logs, computes metric deltas over a user-specified time window (e.g., matching the exact time an incident occurred), and outputs a comparative analysis to instantly identify spikes in specific memory counters (like `pglazyfreed` or `pgfault`).
* **Help Mode (`-help`):** Queries a localized internal database of kernel metrics to explain their definitions, units, and potential impacts, empowering users with immediate, actionable context.

### Intended Users
This tool is expressly built for:
* **Google Compute Engine (GCE) Customers:** To run proactively or reactively within their VMs for self-service diagnosis and localized troubleshooting without needing deep kernel expertise.
* **Google Cloud Support & Engineering Teams:** To securely and standardizedly gather PII-scrubbed diagnostic data during support escalations, eliminating the reliance on custom, ad-hoc shell scripts.

## Deployment and Terms

This tool is designed to be executed within [Google Compute Engine (GCE) Virtual Machines](https://cloud.google.com/compute). By utilizing this software within your Google Cloud environment, your usage is subject to the [Google Cloud Platform Terms of Service](https://cloud.google.com/terms).

---

## Disclaimers

This is not an officially supported Google product. This project is not eligible for the [Google Open Source Software Vulnerability Rewards Program](https://bughunters.google.com/open-source-security).