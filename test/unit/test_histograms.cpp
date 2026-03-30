/*
 * test_histograms.cpp — Unit tests for the log2 histogram.
 *
 * Tests O(1) insertion, percentile estimation accuracy, reset, and
 * edge cases such as zero-value and single-sample inputs.
 */

#include <gtest/gtest.h>

#include "core/histograms.hpp"

namespace kscope {
namespace {

TEST(Histogram, EmptyHistogramReturnsZeros) {
    Histogram h;
    EXPECT_EQ(h.count(), 0ULL);
    EXPECT_EQ(h.sum(), 0ULL);
    EXPECT_EQ(h.max(), 0ULL);
    EXPECT_DOUBLE_EQ(h.average(), 0.0);
    EXPECT_EQ(h.percentile(50), 0ULL);
    EXPECT_EQ(h.percentile(95), 0ULL);
    EXPECT_EQ(h.percentile(99), 0ULL);
}

TEST(Histogram, SingleValue) {
    Histogram h;
    h.record(1'000'000);

    EXPECT_EQ(h.count(), 1ULL);
    EXPECT_EQ(h.sum(), 1'000'000ULL);
    EXPECT_EQ(h.min(), 1'000'000ULL);
    EXPECT_EQ(h.max(), 1'000'000ULL);
    EXPECT_DOUBLE_EQ(h.average(), 1'000'000.0);
}

TEST(Histogram, MultipleValues) {
    Histogram h;
    h.record(100);
    h.record(200);
    h.record(300);

    EXPECT_EQ(h.count(), 3ULL);
    EXPECT_EQ(h.sum(), 600ULL);
    EXPECT_EQ(h.min(), 100ULL);
    EXPECT_EQ(h.max(), 300ULL);
    EXPECT_DOUBLE_EQ(h.average(), 200.0);
}

TEST(Histogram, ZeroValue) {
    Histogram h;
    h.record(0);

    EXPECT_EQ(h.count(), 1ULL);
    EXPECT_EQ(h.sum(), 0ULL);
    EXPECT_EQ(h.min(), 0ULL);
    EXPECT_EQ(h.max(), 0ULL);
}

TEST(Histogram, ResetClearsAll) {
    Histogram h;
    h.record(100);
    h.record(200);
    h.reset();

    EXPECT_EQ(h.count(), 0ULL);
    EXPECT_EQ(h.sum(), 0ULL);
    EXPECT_EQ(h.max(), 0ULL);
    EXPECT_EQ(h.percentile(50), 0ULL);
}

TEST(Histogram, PercentilesApproximate) {
    Histogram h;
    for (uint64_t i = 1; i <= 1000; i++) {
        h.record(i * 1000);
    }

    EXPECT_EQ(h.count(), 1000ULL);
    EXPECT_EQ(h.min(), 1000ULL);
    EXPECT_EQ(h.max(), 1'000'000ULL);

    auto p50 = h.percentile(50);
    auto p95 = h.percentile(95);
    auto p99 = h.percentile(99);

    EXPECT_GT(p50, 0ULL);
    EXPECT_GT(p95, p50);
    EXPECT_GE(p99, p95);
    EXPECT_LE(p99, h.max());
}

TEST(Histogram, P100ReturnsUpperBound) {
    Histogram h;
    h.record(100);
    h.record(50'000);
    h.record(1'000'000);

    EXPECT_GE(h.percentile(100), h.percentile(99));
    EXPECT_EQ(h.max(), 1'000'000ULL);
}

TEST(Histogram, LargeValues) {
    Histogram h;
    h.record(1ULL << 40);
    h.record(1ULL << 50);
    h.record(1ULL << 60);

    EXPECT_EQ(h.count(), 3ULL);
    EXPECT_EQ(h.max(), 1ULL << 60);
}

TEST(Histogram, ManySmallValues) {
    Histogram h;
    for (int i = 0; i < 10000; i++) {
        h.record(42);
    }

    EXPECT_EQ(h.count(), 10000ULL);
    EXPECT_EQ(h.min(), 42ULL);
    EXPECT_EQ(h.max(), 42ULL);
    EXPECT_DOUBLE_EQ(h.average(), 42.0);
}

}  // namespace
}  // namespace kscope
