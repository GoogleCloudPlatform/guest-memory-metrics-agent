#include "third_party/guest_memory_metrics_agent/engine/help_database.h"

#include <string>

#include "testing/base/public/gunit.h"

namespace guest_memory_metrics {
namespace {

TEST(HelpDatabaseTest, KnownMetricReturnsHelpText) {
  // Test that calling it with a known metric returns a non-empty string
  // that isn't the default fallback. Using "pgmajfault" as the known metric.
  std::string result = GetHelpForMetric("pgmajfault");
  EXPECT_FALSE(result.empty());
  EXPECT_NE(result, "No help available for this metric.");
}

TEST(HelpDatabaseTest, UnknownMetricReturnsFallbackText) {
  // Test that calling it with an unknown metric returns the fallback default
  // string.
  std::string result = GetHelpForMetric("UnknownMetric123");
  EXPECT_EQ(result, "No help available for this metric.");
}

}  // namespace
}  // namespace guest_memory_metrics
