#!/bin/bash
# Copyright 2026 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# record_metrics.sh
# Script to build and run the Guest Memory Metrics Agent in record mode
# Note: This script is intended to be executed from your workspace
# on your host or dev machine. It will build the static binary locally,
# then execute it (using sudo since sampling requires CAP_DAC_READ_SEARCH).
# For pure guest VM execution, you only need to copy the resulting compiled
# binary to the guest and execute it directly.

set -euo pipefail

# Set default values or use provided arguments
DURATION="${1:-1m}"       # Default to 1 minute recording
INTERVAL="${2:-5s}"       # Default to 5 second sampling intervals
OUTPUT_LOG="${3:-/tmp/memory_metrics.log}"

echo "Building the Guest Memory Metrics Agent..."
# Fallback between Bazel and Blaze
BUILD_CMD="${BUILD_CMD:-$(command -v bazel || command -v blaze)}"
$BUILD_CMD build --features=fully_static_link //:kernel_metrics_agent

echo "Starting record mode for $DURATION with $INTERVAL sampling intervals."
echo "Output will be saved to: $OUTPUT_LOG"
echo "------------------------------------------------------------------"

# Find compiled binary dynamically and copy to /tmp to avoid root/symlink permission issues
AGENT_BIN=$(find -L . -name kernel_metrics_agent -type f 2>/dev/null | head -n 1)
cp "$AGENT_BIN" /tmp/kernel_metrics_agent

# Run the agent in record mode (sudo required to read restricted metrics)
sudo /tmp/kernel_metrics_agent record \
    --memory_duration="$DURATION" \
    --sample="$INTERVAL" \
    --output="$OUTPUT_LOG"

echo "------------------------------------------------------------------"
echo "Recording complete!"
echo "To view your percent change calculations, use the report mode like this:"
echo "/tmp/kernel_metrics_agent report \\"
echo "    --input=$OUTPUT_LOG \\"
echo "    --start=<start_timestamp> \\"
echo "    --end=<end_timestamp>"
