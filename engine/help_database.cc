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

#include "engine/help_database.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <string_view>

namespace guest_memory_metrics {

std::string GetHelpForMetric(const std::string& metric_name) {
  struct HelpEntry {
    std::string_view key;
    std::string_view description;
  };

  // MUST be kept sorted alphabetically by key for std::lower_bound binary
  // search lookup.
  static constexpr HelpEntry kHelpTable[] = {
      {"Active",
       "Memory that has been used more recently and usually not reclaimed "
       "unless absolutely necessary."},
      {"Active(anon)", "Anonymous memory that has been used recently."},
      {"Active(file)", "Pagecache memory that has been used recently."},
      {"AnonHugePages",
       "Anonymous hugepages mapped into userspace page tables."},
      {"AnonPages", "Non-file backed pages mapped into userspace page tables."},
      {"Bounce", "Memory used for bounce buffers (usually for old hardware)."},
      {"Buffers",
       "Relatively temporary storage for raw disk blocks that shouldn't get "
       "tremendously large (20MB or so)."},
      {"Cached",
       "In-memory cache for files read from the disk (the page cache). Doesn't "
       "include SwapCached."},
      {"CommitLimit",
       "Based on the overcommit ratio (vm.overcommit_ratio), this is the total "
       "amount of memory currently available to be allocated on the system."},
      {"Committed_AS",
       "The amount of memory presently allocated on the system. The committed "
       "memory is a sum of all of the memory which has been allocated by "
       "processes."},
      {"Dirty", "Memory which is waiting to get written back to the disk."},
      {"FileHugePages", "Memory used for file-backed huge pages."},
      {"FilePmdMapped",
       "File-backed memory mapped into userspace with huge pages."},
      {"HardwareCorrupted",
       "The amount of RAM that the kernel identified as corrupted / not "
       "working."},
      {"HugePages_Free",
       "The number of huge pages in the pool that are not yet allocated."},
      {"HugePages_Rsvd",
       "This is the number of huge pages for which a commitment to allocate "
       "from the pool has been made, but no allocation has yet been made."},
      {"HugePages_Surp",
       "This is the number of huge pages in the pool above the value in "
       "vm.nr_hugepages."},
      {"HugePages_Total", "The size of the pool of huge pages."},
      {"Hugepagesize", "The size of a huge page."},
      {"Hugetlb", "Total amount of memory allocated for huge pages."},
      {"Inactive",
       "Memory which has been less recently used. It is more eligible to be "
       "reclaimed for other purposes."},
      {"Inactive(anon)", "Anonymous memory that has not been used recently."},
      {"Inactive(file)", "Pagecache memory that has not been used recently."},
      {"KernelStack", "Memory consumed by the kernel stacks of all tasks."},
      {"Mapped", "Files which have been mmapped, such as libraries."},
      {"MemAvailable",
       "An estimate of how much memory is available for starting new "
       "applications, without swapping. Calculated from MemFree, Cached, "
       "Buffers, and Slab."},
      {"MemFree",
       "The sum of LowFree+HighFree. Amount of physical RAM left unused by the "
       "system."},
      {"MemTotal",
       "Total usable RAM (i.e. physical RAM minus a few reserved bits and the "
       "kernel binary code)."},
      {"Mlocked", "Pages locked to memory using the mlock() system call."},
      {"NFS_Unstable",
       "NFS pages sent to the server, but not yet committed to stable "
       "storage."},
      {"PageTables", "Memory consumed by userspace page tables."},
      {"Percpu",
       "Memory allocated to the percpu allocator used to back percpu "
       "allocations."},
      {"SReclaimable",
       "Part of Slab, that might be reclaimed, such as caches."},
      {"SUnreclaim",
       "Part of Slab, that cannot be reclaimed on memory pressure."},
      {"Shmem", "Total memory used by shared memory (shmem) and tmpfs."},
      {"ShmemHugePages",
       "Memory used by shared memory (shmem) and tmpfs allocated with huge "
       "pages."},
      {"ShmemPmdMapped",
       "Shared memory mapped into userspace with huge pages."},
      {"Slab", "In-kernel data structures cache."},
      {"SwapCached",
       "Memory that once was swapped out, is swapped back in but still also is "
       "in the swap file. If memory pressure arises, these pages don't need to "
       "be swapped out again."},
      {"SwapFree",
       "Memory which has been evicted from RAM, and is temporarily on the "
       "disk."},
      {"SwapTotal", "Total amount of swap space available."},
      {"Unevictable",
       "Memory allocated for userspace which cannot be evicted (e.g. mlocked, "
       "ramfs, etc)."},
      {"VmallocChunk",
       "Largest contiguous block of vmalloc area which is free."},
      {"VmallocTotal", "Total size of vmalloc memory area."},
      {"VmallocUsed", "Amount of vmalloc area which is used."},
      {"Writeback", "Memory which is actively being written back to the disk."},
      {"WritebackTmp", "Memory used by FUSE for temporary writeback buffers."},
      {"pglazyfreed",
       "Details regarding madvise(MADV_FREE) performance impacts and "
       "mitigation suggestions. Indicates memory pages that have been marked "
       "as freeable but haven't been fully reclaimed yet. High values could "
       "point to aggressive memory management or lazy freeing."},
      {"pgmajfault",
       "Indicates the number of major page faults, meaning data had to be "
       "fetched from disk. High values usually indicate memory pressure "
       "causing the system to swap or read excessively from disk."}};

  auto cmp = [](const HelpEntry& a, std::string_view target_key) {
    return a.key < target_key;
  };

  auto it = std::lower_bound(std::begin(kHelpTable), std::end(kHelpTable),
                             metric_name, cmp);

  if (it != std::end(kHelpTable) && it->key == metric_name) {
    return std::string(it->description);
  }

  return "No help available for this metric.";
}

}  // namespace guest_memory_metrics
