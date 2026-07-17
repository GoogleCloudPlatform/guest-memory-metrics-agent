#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <filesystem>  // NOLINT
#include <fstream>
#include <limits>
#include <string>
#include <system_error>  // NOLINT(build/c++11)
#include <thread>        // NOLINT(build/c++11)

#include "testing/base/public/gunit.h"
#include "third_party/guest_memory_metrics_agent/providers/cgroup_provider.h"
#include "third_party/guest_memory_metrics_agent/providers/metric_snapshot.h"
#include "third_party/guest_memory_metrics_agent/providers/numa_provider.h"
#include "third_party/guest_memory_metrics_agent/providers/proc_provider.h"

namespace guest_memory_metrics {
namespace {

namespace fs = std::filesystem;

class ProvidersTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* test_tmpdir = std::getenv("TEST_TMPDIR");
    fs::path base_temp =
        test_tmpdir ? fs::path(test_tmpdir) : fs::temp_directory_path();
    const auto* test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    temp_dir_ = base_temp / test_info->name();
    fs::create_directories(temp_dir_);
  }

  void TearDown() override { fs::remove_all(temp_dir_); }

  fs::path temp_dir_;
};

TEST_F(ProvidersTest, ProcProviderTest) {
  fs::path proc_dir = temp_dir_ / "proc";
  fs::create_directories(proc_dir);

  {
    std::ofstream meminfo(proc_dir / "meminfo");
    meminfo << "MemTotal:       16396536 kB\n";
    meminfo << "MemFree:         9876544 kB\n";
    meminfo << "Buffers:          123456 kB\n";
  }

  {
    std::ofstream vmstat(proc_dir / "vmstat");
    vmstat << "nr_free_pages 2469136\n";
    vmstat << "nr_inactive_anon 5000\n";
  }

  ProcProvider provider(proc_dir.string());
  MetricSnapshot snapshot = provider.GetSnapshot();

  EXPECT_EQ(snapshot.metrics["proc.meminfo.MemTotal"], 16396536ULL * 1024);
  EXPECT_EQ(snapshot.metrics["proc.meminfo.MemFree"], 9876544ULL * 1024);
  EXPECT_EQ(snapshot.metrics["proc.meminfo.Buffers"], 123456ULL * 1024);

  EXPECT_EQ(snapshot.metrics["proc.vmstat.nr_free_pages"], 2469136ULL);
  EXPECT_EQ(snapshot.metrics["proc.vmstat.nr_inactive_anon"], 5000ULL);
}

TEST_F(ProvidersTest, ProcProviderMissingFile) {
  fs::path proc_dir = temp_dir_ / "proc_missing";
  fs::create_directories(proc_dir);

  ProcProvider provider(proc_dir.string());
  MetricSnapshot snapshot = provider.GetSnapshot();

  EXPECT_TRUE(snapshot.metrics.empty());
}

TEST_F(ProvidersTest, CgroupV2_Basic) {
  fs::path cgroup_dir = temp_dir_ / "cgroup";
  fs::create_directories(cgroup_dir);

  {
    std::ofstream current(cgroup_dir / "memory.current");
    current << "1000000\n";
  }
  {
    std::ofstream max(cgroup_dir / "memory.max");
    max << "2000000\n";
  }

  CgroupProvider provider(cgroup_dir.string());
  MetricSnapshot snapshot = provider.GetSnapshot();

  EXPECT_EQ(snapshot.metrics["cgroup.memory.current"], 1000000ULL);
  EXPECT_EQ(snapshot.metrics["cgroup.memory.max"], 2000000ULL);
}

TEST_F(ProvidersTest, CgroupV2_MaxLiteral) {
  fs::path cgroup_dir = temp_dir_ / "cgroup";
  fs::create_directories(cgroup_dir);
  {
    std::ofstream max(cgroup_dir / "memory.max");
    max << "max\n";
  }
  CgroupProvider provider(cgroup_dir.string());
  MetricSnapshot snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.find("cgroup.memory.max") ==
                  snapshot.metrics.end() ||
              snapshot.metrics["cgroup.memory.max"] ==
                  std::numeric_limits<uint64_t>::max());
}

TEST_F(ProvidersTest, CgroupV2_NestedSubgroups) {
  fs::path cgroup_dir = temp_dir_ / "cgroup";
  fs::path sub_cgroup_dir = cgroup_dir / "subgroup" / "subsubgroup";
  fs::create_directories(sub_cgroup_dir);
  {
    std::ofstream current(sub_cgroup_dir / "memory.current");
    current << "500000\n";
  }
  CgroupProvider provider(cgroup_dir.string());
  MetricSnapshot snapshot = provider.GetSnapshot();
  EXPECT_EQ(snapshot.metrics["cgroup.subgroup/subsubgroup/memory.current"],
            500000ULL);
}

TEST_F(ProvidersTest, CgroupV2_MissingFiles) {
  fs::path cgroup_dir = temp_dir_ / "cgroup_missing";
  fs::create_directories(cgroup_dir);
  CgroupProvider provider(cgroup_dir.string());
  MetricSnapshot snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.empty());
}

TEST_F(ProvidersTest, CgroupV2_ZeroValues) {
  fs::path cgroup_dir = temp_dir_ / "cgroup";
  fs::create_directories(cgroup_dir);
  {
    std::ofstream current(cgroup_dir / "memory.current");
    current << "0\n";
  }
  CgroupProvider provider(cgroup_dir.string());
  MetricSnapshot snapshot = provider.GetSnapshot();
  EXPECT_EQ(snapshot.metrics["cgroup.memory.current"], 0ULL);
}

TEST_F(ProvidersTest, CgroupV2_GarbageStrings) {
  fs::path cgroup_dir = temp_dir_ / "cgroup";
  fs::create_directories(cgroup_dir);
  {
    std::ofstream current(cgroup_dir / "memory.current");
    current << "garbage\n";
  }
  CgroupProvider provider(cgroup_dir.string());
  MetricSnapshot snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.find("cgroup.memory.current") ==
              snapshot.metrics.end());
}

TEST_F(ProvidersTest, CgroupV2_MissingValue) {
  fs::path cgroup_dir = temp_dir_ / "cgroup";
  fs::create_directories(cgroup_dir);
  {
    std::ofstream stat(cgroup_dir / "memory.stat");
    stat << "anon \n";
  }
  CgroupProvider provider(cgroup_dir.string());
  MetricSnapshot snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.find("cgroup.memory.stat.anon") ==
              snapshot.metrics.end());
}

TEST_F(ProvidersTest, CgroupV1_Basic) {
  fs::path cgroup_dir = temp_dir_ / "cgroup";
  fs::create_directories(cgroup_dir);
  {
    std::ofstream usage(cgroup_dir / "memory.usage_in_bytes");
    usage << "1000000\n";
  }
  {
    std::ofstream limit(cgroup_dir / "memory.limit_in_bytes");
    limit << "2000000\n";
  }
  {
    std::ofstream stat(cgroup_dir / "memory.stat");
    stat << "cache 5000\n";
  }
  CgroupProvider provider(cgroup_dir.string());
  MetricSnapshot snapshot = provider.GetSnapshot();
  EXPECT_EQ(snapshot.metrics["cgroup.memory.usage_in_bytes"], 1000000ULL);
  EXPECT_EQ(snapshot.metrics["cgroup.memory.limit_in_bytes"], 2000000ULL);
  EXPECT_EQ(snapshot.metrics["cgroup.memory.stat.cache"], 5000ULL);
}

TEST_F(ProvidersTest, CgroupV1_HierarchicalStats) {
  fs::path cgroup_dir = temp_dir_ / "cgroup";
  fs::create_directories(cgroup_dir);
  {
    std::ofstream stat(cgroup_dir / "memory.stat");
    stat << "cache 5000\n";
    stat << "total_cache 10000\n";
  }
  CgroupProvider provider(cgroup_dir.string());
  MetricSnapshot snapshot = provider.GetSnapshot();
  EXPECT_EQ(snapshot.metrics["cgroup.memory.stat.cache"], 5000ULL);
  EXPECT_EQ(snapshot.metrics["cgroup.memory.stat.total_cache"], 10000ULL);
}

TEST_F(ProvidersTest, CgroupV1_UnlimitedValues) {
  fs::path cgroup_dir = temp_dir_ / "cgroup";
  fs::create_directories(cgroup_dir);
  {
    std::ofstream limit(cgroup_dir / "memory.limit_in_bytes");
    limit << "9223372036854771712\n";
  }
  CgroupProvider provider(cgroup_dir.string());
  MetricSnapshot snapshot = provider.GetSnapshot();
  EXPECT_EQ(snapshot.metrics["cgroup.memory.limit_in_bytes"],
            9223372036854771712ULL);
}

TEST_F(ProvidersTest, CgroupV1_NestedSubgroups) {
  fs::path cgroup_dir = temp_dir_ / "cgroup";
  fs::path sub_cgroup_dir = cgroup_dir / "subgroup";
  fs::create_directories(sub_cgroup_dir);
  {
    std::ofstream usage(sub_cgroup_dir / "memory.usage_in_bytes");
    usage << "500000\n";
  }
  CgroupProvider provider(cgroup_dir.string());
  MetricSnapshot snapshot = provider.GetSnapshot();
  EXPECT_EQ(snapshot.metrics["cgroup.subgroup/memory.usage_in_bytes"],
            500000ULL);
}

TEST_F(ProvidersTest, CgroupV1_MissingFiles) {
  fs::path cgroup_dir = temp_dir_ / "cgroup";
  fs::create_directories(cgroup_dir);
  CgroupProvider provider(cgroup_dir.string());
  MetricSnapshot snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.empty());
}

TEST_F(ProvidersTest, CgroupV1_IrregularWhitespaceGarbage) {
  fs::path cgroup_dir = temp_dir_ / "cgroup";
  fs::create_directories(cgroup_dir);
  {
    std::ofstream stat(cgroup_dir / "memory.stat");
    stat << " \t cache \t 5000 \t \n";
    stat << "\n";
    stat << "garbage\n";
  }
  CgroupProvider provider(cgroup_dir.string());
  MetricSnapshot snapshot = provider.GetSnapshot();
  EXPECT_EQ(snapshot.metrics["cgroup.memory.stat.cache"], 5000ULL);
}

TEST_F(ProvidersTest, CgroupV1_CorruptedData) {
  fs::path cgroup_dir = temp_dir_ / "cgroup";
  fs::create_directories(cgroup_dir);
  {
    std::ofstream stat(cgroup_dir / "memory.stat");
    stat << std::string("\0\0\0\0", 4);
  }
  CgroupProvider provider(cgroup_dir.string());
  MetricSnapshot snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.empty());
}

TEST_F(ProvidersTest, Universal_SkipNoReadAccess) {
  fs::path cgroup_dir = temp_dir_ / "cgroup";
  fs::create_directories(cgroup_dir);
  fs::path secret_dir = cgroup_dir / "secret";
  fs::create_directories(secret_dir);
  fs::permissions(secret_dir, fs::perms::none);

  {
    std::ofstream current(cgroup_dir / "memory.current");
    current << "1000\n";
  }

  CgroupProvider provider(cgroup_dir.string());
  MetricSnapshot snapshot = provider.GetSnapshot();

  fs::permissions(secret_dir, fs::perms::all);

  EXPECT_EQ(snapshot.metrics["cgroup.memory.current"], 1000ULL);
  EXPECT_TRUE(snapshot.metrics.find("cgroup.secret/memory.current") ==
              snapshot.metrics.end());
}

TEST_F(ProvidersTest, Universal_EmptyFile) {
  fs::path cgroup_dir = temp_dir_ / "cgroup";
  fs::create_directories(cgroup_dir);
  {
    std::ofstream current(cgroup_dir / "memory.current");
  }
  CgroupProvider provider(cgroup_dir.string());
  MetricSnapshot snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.empty());
}

TEST_F(ProvidersTest, Universal_RaceConditionDirDestroyed) {
  fs::path cgroup_dir = temp_dir_ / "cgroup_race";
  fs::create_directories(cgroup_dir);

  // Create a large number of files so GetSnapshot takes enough time to race
  for (int i = 0; i < 500; ++i) {
    fs::path sub_dir = cgroup_dir / ("sub_" + std::to_string(i));
    fs::create_directories(sub_dir);
    std::ofstream current(sub_dir / "memory.current");
    current << "1000\n";
  }

  CgroupProvider provider(cgroup_dir.string());

  std::atomic<bool> start_delete{false};
  std::thread deleter([&]() {
    while (!start_delete) {
      std::this_thread::yield();
    }
    std::error_code ec;
    fs::remove_all(cgroup_dir, ec);
  });

  start_delete = true;
  MetricSnapshot snapshot = provider.GetSnapshot();

  deleter.join();  // Block until the deleter thread finishes.

  // The snapshot should either be empty, or partially filled, but it MUST NOT
  // crash.
  SUCCEED();
}

TEST_F(ProvidersTest, Universal_SymlinkLoop) {
  fs::path cgroup_dir = temp_dir_ / "cgroup";
  fs::create_directories(cgroup_dir);

  fs::create_directory_symlink(cgroup_dir, cgroup_dir / "loop");

  {
    std::ofstream current(cgroup_dir / "memory.current");
    current << "1000\n";
  }

  CgroupProvider provider(cgroup_dir.string());
  MetricSnapshot snapshot = provider.GetSnapshot();

  EXPECT_EQ(snapshot.metrics["cgroup.memory.current"], 1000ULL);

  fs::remove(cgroup_dir / "loop");
}

TEST_F(ProvidersTest, VersionDetection_PureV2) {
  fs::path cgroup_dir = temp_dir_ / "cgroup";
  fs::create_directories(cgroup_dir);
  {
    std::ofstream current(cgroup_dir / "memory.current");
    current << "1000\n";
  }
  CgroupProvider provider(cgroup_dir.string());
  MetricSnapshot snapshot = provider.GetSnapshot();
  EXPECT_EQ(snapshot.metrics["cgroup.memory.current"], 1000ULL);
}

TEST_F(ProvidersTest, VersionDetection_PureV1) {
  fs::path cgroup_dir = temp_dir_ / "cgroup";
  fs::create_directories(cgroup_dir);
  {
    std::ofstream usage(cgroup_dir / "memory.usage_in_bytes");
    usage << "1000\n";
  }
  CgroupProvider provider(cgroup_dir.string());
  MetricSnapshot snapshot = provider.GetSnapshot();
  EXPECT_EQ(snapshot.metrics["cgroup.memory.usage_in_bytes"], 1000ULL);
}

TEST_F(ProvidersTest, VersionDetection_MixedFallback) {
  fs::path cgroup_dir = temp_dir_ / "cgroup";
  fs::create_directories(cgroup_dir);
  {
    std::ofstream current(cgroup_dir / "memory.current");
    current << "1000\n";
  }
  {
    std::ofstream usage(cgroup_dir / "memory.usage_in_bytes");
    usage << "2000\n";
  }
  CgroupProvider provider(cgroup_dir.string());
  MetricSnapshot snapshot = provider.GetSnapshot();
  // V2 points are present, we should prefer them and suppress V1
  EXPECT_EQ(snapshot.metrics["cgroup.memory.current"], 1000ULL);
  EXPECT_TRUE(snapshot.metrics.find("cgroup.memory.usage_in_bytes") ==
              snapshot.metrics.end());
}

TEST_F(ProvidersTest, VersionDetection_MixedFallback_Max) {
  fs::path cgroup_dir = temp_dir_ / "cgroup";
  fs::create_directories(cgroup_dir);
  {
    std::ofstream max(cgroup_dir / "memory.max");
    max << "3000\n";
  }
  {
    std::ofstream limit(cgroup_dir / "memory.limit_in_bytes");
    limit << "4000\n";
  }
  CgroupProvider provider(cgroup_dir.string());
  MetricSnapshot snapshot = provider.GetSnapshot();
  EXPECT_EQ(snapshot.metrics["cgroup.memory.max"], 3000ULL);
  EXPECT_TRUE(snapshot.metrics.find("cgroup.memory.limit_in_bytes") ==
              snapshot.metrics.end());
}

TEST_F(ProvidersTest, VersionDetection_MixedFallback_Subgroups) {
  fs::path cgroup_dir = temp_dir_ / "cgroup";
  fs::path sub_cgroup_dir = cgroup_dir / "subgroup";
  fs::create_directories(sub_cgroup_dir);
  {
    std::ofstream current(sub_cgroup_dir / "memory.current");
    current << "1000\n";
  }
  {
    std::ofstream usage(sub_cgroup_dir / "memory.usage_in_bytes");
    usage << "2000\n";
  }
  {
    std::ofstream max(sub_cgroup_dir / "memory.max");
    max << "3000\n";
  }
  {
    std::ofstream limit(sub_cgroup_dir / "memory.limit_in_bytes");
    limit << "4000\n";
  }
  CgroupProvider provider(cgroup_dir.string());
  MetricSnapshot snapshot = provider.GetSnapshot();
  EXPECT_EQ(snapshot.metrics["cgroup.subgroup/memory.current"], 1000ULL);
  EXPECT_TRUE(snapshot.metrics.find("cgroup.subgroup/memory.usage_in_bytes") ==
              snapshot.metrics.end());
  EXPECT_EQ(snapshot.metrics["cgroup.subgroup/memory.max"], 3000ULL);
  EXPECT_TRUE(snapshot.metrics.find("cgroup.subgroup/memory.limit_in_bytes") ==
              snapshot.metrics.end());
}

TEST_F(ProvidersTest, NumaProviderTest) {
  fs::path sys_dir = temp_dir_ / "sys_devices_system_node";
  fs::path node0_dir = sys_dir / "node0";
  fs::path node1_dir = sys_dir / "node1";
  fs::create_directories(node0_dir);
  fs::create_directories(node1_dir);

  {
    std::ofstream meminfo(node0_dir / "meminfo");
    meminfo << "Node 0 MemTotal:        1024 kB\n";
    meminfo << "Node 0 MemFree:          512 kB\n";
  }

  {
    std::ofstream meminfo(node1_dir / "meminfo");
    meminfo << "Node 1 MemTotal:        2048 kB\n";
    meminfo << "Node 1 MemFree:          256 kB\n";
  }

  NumaProvider provider(sys_dir.string());
  MetricSnapshot snapshot = provider.GetSnapshot();

  EXPECT_EQ(snapshot.metrics["numa.node0.MemTotal"], 1024ULL * 1024);
  EXPECT_EQ(snapshot.metrics["numa.node0.MemFree"], 512ULL * 1024);
  EXPECT_EQ(snapshot.metrics["numa.node1.MemTotal"], 2048ULL * 1024);
  EXPECT_EQ(snapshot.metrics["numa.node1.MemFree"], 256ULL * 1024);
}

// --- ProcProvider Edge Cases ---
TEST_F(ProvidersTest, ProcProviderIrregularWhitespace) {
  fs::path proc_dir = temp_dir_ / "proc_edge";
  fs::create_directories(proc_dir);
  std::ofstream(proc_dir / "meminfo") << "MemTotal:\t \t  16396536\t \tkB\n";
  ProcProvider provider(proc_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_EQ(snapshot.metrics["proc.meminfo.MemTotal"], 16396536ULL * 1024);
}

TEST_F(ProvidersTest, ProcProviderMissingValues) {
  fs::path proc_dir = temp_dir_ / "proc_edge";
  fs::create_directories(proc_dir);
  std::ofstream(proc_dir / "meminfo") << "Buffers:\n";
  ProcProvider provider(proc_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.find("proc.meminfo.Buffers") ==
              snapshot.metrics.end());
}

TEST_F(ProvidersTest, ProcProviderNonIntegerStringValues) {
  fs::path proc_dir = temp_dir_ / "proc_edge";
  fs::create_directories(proc_dir);
  std::ofstream(proc_dir / "meminfo") << "Cached:         garbage kB\n";
  ProcProvider provider(proc_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.find("proc.meminfo.Cached") ==
              snapshot.metrics.end());
}

TEST_F(ProvidersTest, ProcProviderUnrecognizedUnits) {
  fs::path proc_dir = temp_dir_ / "proc_edge";
  fs::create_directories(proc_dir);
  std::ofstream(proc_dir / "meminfo") << "SwapCached:     1024 garBage\n";
  ProcProvider provider(proc_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_EQ(snapshot.metrics["proc.meminfo.SwapCached"], 1024ULL);
}

TEST_F(ProvidersTest, ProcProviderGarbageLines) {
  fs::path proc_dir = temp_dir_ / "proc_edge";
  fs::create_directories(proc_dir);
  std::ofstream(proc_dir / "meminfo") << "GarbageLineWithoutValue\n";
  ProcProvider provider(proc_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.find("proc.meminfo.GarbageLineWithoutValue") ==
              snapshot.metrics.end());
}

TEST_F(ProvidersTest, ProcProviderEmptyLines) {
  fs::path proc_dir = temp_dir_ / "proc_edge";
  fs::create_directories(proc_dir);
  std::ofstream(proc_dir / "meminfo") << "\n";
  ProcProvider provider(proc_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.empty());
}

TEST_F(ProvidersTest, ProcProviderNegativeNumbers) {
  fs::path proc_dir = temp_dir_ / "proc_edge";
  fs::create_directories(proc_dir);
  std::ofstream(proc_dir / "meminfo") << "CommitLimit:    -1 kB\n";
  ProcProvider provider(proc_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.contains("proc.meminfo.CommitLimit"));
  EXPECT_EQ(snapshot.metrics["proc.meminfo.CommitLimit"], static_cast<uint64_t>(-1024));
}

TEST_F(ProvidersTest, ProcProviderEmptyFiles) {
  fs::path proc_dir = temp_dir_ / "proc_edge";
  fs::create_directories(proc_dir);
  std::ofstream(proc_dir / "vmstat") << "";
  ProcProvider provider(proc_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.empty());
}

TEST_F(ProvidersTest, ProcProviderInaccessibleFiles) {
  fs::path proc_dir = temp_dir_ / "proc_edge";
  fs::create_directories(proc_dir);
  fs::path file_path = proc_dir / "vmstat";
  std::ofstream(file_path) << "nr_free_pages 2469136\n";
  fs::permissions(file_path, fs::perms::none);
  ProcProvider provider(proc_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.empty());
  fs::permissions(file_path, fs::perms::owner_all);
}

TEST_F(ProvidersTest, ProcProviderDetachedColons) {
  fs::path proc_dir = temp_dir_ / "proc_edge";
  fs::create_directories(proc_dir);
  std::ofstream(proc_dir / "meminfo") << "Inactive :      1024 kB\n";
  ProcProvider provider(proc_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.find("proc.meminfo.Inactive") ==
              snapshot.metrics.end());
}

TEST_F(ProvidersTest, ProcProviderMissingUnits) {
  fs::path proc_dir = temp_dir_ / "proc_edge";
  fs::create_directories(proc_dir);
  std::ofstream(proc_dir / "meminfo") << "Active:         2048\n";
  ProcProvider provider(proc_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_EQ(snapshot.metrics["proc.meminfo.Active"], 2048ULL);
}

// --- CgroupProvider Edge Cases ---
TEST_F(ProvidersTest, CgroupProviderIrregularWhitespace) {
  fs::path cgroup_dir = temp_dir_ / "cgroup_edge";
  fs::create_directories(cgroup_dir);
  std::ofstream(cgroup_dir / "memory.stat") << "shmem\t \t 2048 \t \n";
  CgroupProvider provider(cgroup_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_EQ(snapshot.metrics["cgroup.memory.stat.shmem"], 2048ULL);
}

TEST_F(ProvidersTest, CgroupProviderMissingValues) {
  fs::path cgroup_dir = temp_dir_ / "cgroup_edge";
  fs::create_directories(cgroup_dir);
  std::ofstream(cgroup_dir / "memory.stat") << "kernel\n";
  CgroupProvider provider(cgroup_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.find("cgroup.memory.stat.kernel") ==
              snapshot.metrics.end());
}

TEST_F(ProvidersTest, CgroupProviderNonIntegerStringValues) {
  fs::path cgroup_dir = temp_dir_ / "cgroup_edge";
  fs::create_directories(cgroup_dir);
  std::ofstream(cgroup_dir / "memory.current") << "garbage\n";
  CgroupProvider provider(cgroup_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.find("cgroup.memory.current") ==
              snapshot.metrics.end());
}

TEST_F(ProvidersTest, CgroupProviderMaxKeyword) {
  fs::path cgroup_dir = temp_dir_ / "cgroup_edge";
  fs::create_directories(cgroup_dir);
  std::ofstream(cgroup_dir / "memory.max") << "max\n";
  CgroupProvider provider(cgroup_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.find("cgroup.memory.max") ==
              snapshot.metrics.end());
}

TEST_F(ProvidersTest, CgroupProviderEmptyFiles) {
  fs::path cgroup_dir = temp_dir_ / "cgroup_edge";
  fs::create_directories(cgroup_dir);
  std::ofstream(cgroup_dir / "memory.current") << "";
  CgroupProvider provider(cgroup_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.find("cgroup.memory.current") ==
              snapshot.metrics.end());
}

TEST_F(ProvidersTest, CgroupProviderMissingFilesInSubdirectories) {
  fs::path cgroup_dir = temp_dir_ / "cgroup_edge";
  fs::create_directories(cgroup_dir);
  fs::create_directories(cgroup_dir / "empty_subdir");
  CgroupProvider provider(cgroup_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.find("cgroup.empty_subdir/memory.current") ==
              snapshot.metrics.end());
}

TEST_F(ProvidersTest, CgroupProviderInaccessibleFiles) {
  fs::path cgroup_dir = temp_dir_ / "cgroup_edge";
  fs::create_directories(cgroup_dir);
  fs::path file_path = cgroup_dir / "memory.stat";
  std::ofstream(file_path) << "anon 1000\n";
  fs::permissions(file_path, fs::perms::none);
  CgroupProvider provider(cgroup_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.empty());
  fs::permissions(file_path, fs::perms::owner_all);
}

TEST_F(ProvidersTest, CgroupProviderNegativeNumbers) {
  fs::path cgroup_dir = temp_dir_ / "cgroup_edge";
  fs::create_directories(cgroup_dir);
  std::ofstream(cgroup_dir / "memory.stat") << "negative_val -1\n";
  CgroupProvider provider(cgroup_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.contains("cgroup.memory.stat.negative_val"));
  EXPECT_EQ(snapshot.metrics["cgroup.memory.stat.negative_val"], static_cast<uint64_t>(-1));
}

TEST_F(ProvidersTest, CgroupProviderGarbageEmptyLines) {
  fs::path cgroup_dir = temp_dir_ / "cgroup_edge";
  fs::create_directories(cgroup_dir);
  std::ofstream(cgroup_dir / "memory.stat") << "\nfile garbage\n";
  CgroupProvider provider(cgroup_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.find("cgroup.memory.stat.file") ==
              snapshot.metrics.end());
}

TEST_F(ProvidersTest, CgroupProviderInaccessibleRootDirectories) {
  fs::path cgroup_dir = temp_dir_ / "cgroup_edge";
  fs::create_directories(cgroup_dir);
  fs::permissions(cgroup_dir, fs::perms::none);
  CgroupProvider provider(cgroup_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.empty());
  fs::permissions(cgroup_dir, fs::perms::owner_all);
}

// --- NumaProvider Edge Cases ---
TEST_F(ProvidersTest, NumaProviderIrregularWhitespace) {
  fs::path sys_dir = temp_dir_ / "sys_edge";
  fs::create_directories(sys_dir / "node0");
  std::ofstream(sys_dir / "node0" / "meminfo")
      << "Node 0 MemTotal:\t \t 1024 \tkB\n";
  NumaProvider provider(sys_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_EQ(snapshot.metrics["numa.node0.MemTotal"], 1024ULL * 1024);
}

TEST_F(ProvidersTest, NumaProviderMissingValues) {
  fs::path sys_dir = temp_dir_ / "sys_edge";
  fs::create_directories(sys_dir / "node0");
  std::ofstream(sys_dir / "node0" / "meminfo") << "Node 0 MemFree:\n";
  NumaProvider provider(sys_dir.string());

  auto snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.find("numa.node0.MemFree") ==
              snapshot.metrics.end());
}

TEST_F(ProvidersTest, NumaProviderNonIntegerStringValues) {
  fs::path sys_dir = temp_dir_ / "sys_edge";
  fs::create_directories(sys_dir / "node0");
  std::ofstream(sys_dir / "node0" / "meminfo")
      << "Node 0 MemUsed:         garbage\n";
  NumaProvider provider(sys_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.find("numa.node0.MemUsed") ==
              snapshot.metrics.end());
}

TEST_F(ProvidersTest, NumaProviderMissingFilesInSubdirectories) {
  fs::path sys_dir = temp_dir_ / "sys_edge";
  fs::create_directories(sys_dir / "node1");
  NumaProvider provider(sys_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.find("numa.node1.MemTotal") ==
              snapshot.metrics.end());
}

TEST_F(ProvidersTest, NumaProviderUnrecognizedUnits) {
  fs::path sys_dir = temp_dir_ / "sys_edge";
  fs::create_directories(sys_dir / "node0");
  std::ofstream(sys_dir / "node0" / "meminfo")
      << "Node 0 SwapTotal:       2048 garBage\n";
  NumaProvider provider(sys_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_EQ(snapshot.metrics["numa.node0.SwapTotal"], 2048ULL);
}

TEST_F(ProvidersTest, NumaProviderGarbageLines) {
  fs::path sys_dir = temp_dir_ / "sys_edge";
  fs::create_directories(sys_dir / "node0");
  std::ofstream(sys_dir / "node0" / "meminfo") << "GarbageLineWithoutValue\n";
  NumaProvider provider(sys_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.find("numa.node0.GarbageLineWithoutValue") ==
              snapshot.metrics.end());
}

TEST_F(ProvidersTest, NumaProviderEmptyLines) {
  fs::path sys_dir = temp_dir_ / "sys_edge";
  fs::create_directories(sys_dir / "node0");
  std::ofstream(sys_dir / "node0" / "meminfo") << "\n";
  NumaProvider provider(sys_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.empty());
}

TEST_F(ProvidersTest, NumaProviderEmptyFiles) {
  fs::path sys_dir = temp_dir_ / "sys_edge";
  fs::create_directories(sys_dir / "node0");
  std::ofstream(sys_dir / "node0" / "meminfo") << "";
  NumaProvider provider(sys_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.empty());
}

TEST_F(ProvidersTest, NumaProviderInaccessibleFiles) {
  fs::path sys_dir = temp_dir_ / "sys_edge";
  fs::create_directories(sys_dir / "node0");
  fs::path file_path = sys_dir / "node0" / "meminfo";
  std::ofstream(file_path) << "Node 0 MemTotal:        1024 kB\n";
  fs::permissions(file_path, fs::perms::none);
  NumaProvider provider(sys_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.empty());
  fs::permissions(file_path, fs::perms::owner_all);
}

TEST_F(ProvidersTest, NumaProviderNegativeNumbers) {
  fs::path sys_dir = temp_dir_ / "sys_edge";
  fs::create_directories(sys_dir / "node0");
  std::ofstream(sys_dir / "node0" / "meminfo")
      << "Node 0 CommitLimit:     -1 kB\n";
  NumaProvider provider(sys_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.contains("numa.node0.CommitLimit"));
  EXPECT_EQ(snapshot.metrics["numa.node0.CommitLimit"], static_cast<uint64_t>(-1024));
}

TEST_F(ProvidersTest, NumaProviderDetachedColons) {
  fs::path sys_dir = temp_dir_ / "sys_edge";
  fs::create_directories(sys_dir / "node0");
  std::ofstream(sys_dir / "node0" / "meminfo")
      << "Node 0 Inactive :       2048 kB\n";
  NumaProvider provider(sys_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_TRUE(snapshot.metrics.find("numa.node0.Inactive") ==
              snapshot.metrics.end());
}

TEST_F(ProvidersTest, NumaProviderMissingUnits) {
  fs::path sys_dir = temp_dir_ / "sys_edge";
  fs::create_directories(sys_dir / "node0");
  std::ofstream(sys_dir / "node0" / "meminfo")
      << "Node 0 Active:          4096\n";
  NumaProvider provider(sys_dir.string());
  auto snapshot = provider.GetSnapshot();
  EXPECT_EQ(snapshot.metrics["numa.node0.Active"], 4096ULL);
}

}  // namespace
}  // namespace guest_memory_metrics
