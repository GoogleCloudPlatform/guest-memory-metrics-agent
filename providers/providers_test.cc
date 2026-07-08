#include <cstdlib>
#include <filesystem>  // NOLINT
#include <fstream>
#include <string>

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

TEST_F(ProvidersTest, CgroupProviderTest) {
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

  fs::path sub_cgroup_dir = cgroup_dir / "subgroup";
  fs::create_directories(sub_cgroup_dir);
  {
    std::ofstream current(sub_cgroup_dir / "memory.current");
    current << "500000\n";
  }
  {
    std::ofstream stat(sub_cgroup_dir / "memory.stat");
    stat << "anon 1000\n";
    stat << "file 2000\n";
  }

  CgroupProvider provider(cgroup_dir.string());
  MetricSnapshot snapshot = provider.GetSnapshot();

  EXPECT_EQ(snapshot.metrics["cgroup.memory.current"], 1000000ULL);
  EXPECT_EQ(snapshot.metrics["cgroup.memory.max"], 2000000ULL);
  EXPECT_EQ(snapshot.metrics["cgroup.subgroup/memory.current"], 500000ULL);
  EXPECT_EQ(snapshot.metrics["cgroup.subgroup/memory.stat.anon"], 1000ULL);
  EXPECT_EQ(snapshot.metrics["cgroup.subgroup/memory.stat.file"], 2000ULL);
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

}  // namespace
}  // namespace guest_memory_metrics
