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

#include <string>
#include <unordered_map>

#include "absl/base/no_destructor.h"

namespace guest_memory_metrics {

std::string GetHelpForMetric(const std::string& metric_name) {
  static const absl::NoDestructor<std::unordered_map<std::string, std::string>> help_map({
      {"pglazyfreed",
       "Details regarding madvise(MADV_FREE) performance impacts and "
       "mitigation suggestions. Indicates memory pages that have been marked "
       "as freeable but haven't been fully reclaimed yet. High values could "
       "point to aggressive memory management or lazy freeing."},
      {"pgmajfault",
       "Indicates the number of major page faults, meaning data had to be "
       "fetched from disk. High values usually indicate memory pressure "
       "causing the system to swap or read excessively from disk."},
      {"MemTotal",
       "Total usable RAM (i.e. physical RAM minus a few reserved bits and the "
       "kernel binary code)."},
      {"MemFree",
       "The sum of LowFree+HighFree. Amount of physical RAM left unused by the "
       "system."},
      {"MemAvailable",
       "An estimate of how much memory is available for starting new "
       "applications, without swapping. Calculated from MemFree, Cached, "
       "Buffers, and Slab."},
      {"Buffers",
       "Relatively temporary storage for raw disk blocks that shouldn't get "
       "tremendously large (20MB or so)."},
      {"Cached",
       "In-memory cache for files read from the disk (the page cache). Doesn't "
       "include SwapCached."},
      {"SwapCached",
       "Memory that once was swapped out, is swapped back in but still also is "
       "in the swap file. If memory pressure arises, these pages don't need to "
       "be swapped out again."},
      {"Active",
       "Memory that has been used more recently and usually not reclaimed "
       "unless absolutely necessary."},
      {"Inactive",
       "Memory which has been less recently used. It is more eligible to be "
       "reclaimed for other purposes."},
      {"Active(anon)", "Anonymous memory that has been used recently."},
      {"Inactive(anon)", "Anonymous memory that has not been used recently."},
      {"Active(file)", "Pagecache memory that has been used recently."},
      {"Inactive(file)", "Pagecache memory that has not been used recently."},
      {"Unevictable",
       "Memory allocated for userspace which cannot be evicted (e.g. mlocked, "
       "ramfs, etc)."},
      {"Mlocked", "Pages locked to memory using the mlock() system call."},
      {"SwapTotal", "Total amount of swap space available."},
      {"SwapFree",
       "Memory which has been evicted from RAM, and is temporarily on the "
       "disk."},
      {"Dirty", "Memory which is waiting to get written back to the disk."},
      {"Writeback", "Memory which is actively being written back to the disk."},
      {"AnonPages", "Non-file backed pages mapped into userspace page tables."},
      {"Mapped", "Files which have been mmapped, such as libraries."},
      {"Shmem", "Total memory used by shared memory (shmem) and tmpfs."},
      {"Slab", "In-kernel data structures cache."},
      {"SReclaimable",
       "Part of Slab, that might be reclaimed, such as caches."},
      {"SUnreclaim",
       "Part of Slab, that cannot be reclaimed on memory pressure."},
      {"KernelStack", "Memory consumed by the kernel stacks of all tasks."},
      {"PageTables", "Memory consumed by userspace page tables."},
      {"NFS_Unstable",
       "NFS pages sent to the server, but not yet committed to stable "
       "storage."},
      {"Bounce", "Memory used for bounce buffers (usually for old hardware)."},
      {"WritebackTmp", "Memory used by FUSE for temporary writeback buffers."},
      {"CommitLimit",
       "Based on the overcommit ratio (vm.overcommit_ratio), this is the total "
       "amount of memory currently available to be allocated on the system."},
      {"Committed_AS",
       "The amount of memory presently allocated on the system. The committed "
       "memory is a sum of all of the memory which has been allocated by "
       "processes."},
      {"VmallocTotal", "Total size of vmalloc memory area."},
      {"VmallocUsed", "Amount of vmalloc area which is used."},
      {"VmallocChunk",
       "Largest contiguous block of vmalloc area which is free."},
      {"Percpu",
       "Memory allocated to the percpu allocator used to back percpu "
       "allocations."},
      {"HardwareCorrupted",
       "The amount of RAM that the kernel identified as corrupted / not "
       "working."},
      {"AnonHugePages",
       "Anonymous hugepages mapped into userspace page tables."},
      {"ShmemHugePages",
       "Memory used by shared memory (shmem) and tmpfs allocated with huge "
       "pages."},
      {"ShmemPmdMapped",
       "Shared memory mapped into userspace with huge pages."},
      {"FileHugePages", "Memory used for file-backed huge pages."},
      {"FilePmdMapped",
       "File-backed memory mapped into userspace with huge pages."},
      {"HugePages_Total", "The size of the pool of huge pages."},
      {"HugePages_Free",
       "The number of huge pages in the pool that are not yet allocated."},
      {"HugePages_Rsvd",
       "This is the number of huge pages for which a commitment to allocate "
       "from the pool has been made, but no allocation has yet been made."},
      {"HugePages_Surp",
       "This is the number of huge pages in the pool above the value in "
       "vm.nr_hugepages."},
      {"Hugepagesize", "The size of a huge page."},
      {"Hugetlb", "Total amount of memory allocated for huge pages."},
  });

  auto it = help_map->find(metric_name);
  if (it != help_map->end()) {
    return it->second;
  }
  return "No help available for this metric.";
}

}  // namespace guest_memory_metrics
