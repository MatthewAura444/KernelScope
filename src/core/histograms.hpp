/*
 * histograms.hpp — Log2 histogram and quantile estimation for KernelScope.
 *
 * Provides O(1) insertion latency histograms using power-of-2 buckets
 * and approximate P50/P95/P99 computation from bin counts.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace kscope {

class Histogram {
public:
    static constexpr size_t kBucketCount = 64;

    Histogram();

    void record(uint64_t value_ns);
    void reset();

    uint64_t count() const;
    uint64_t sum() const { return sum_; }
    uint64_t min() const { return min_; }
    uint64_t max() const { return max_; }
    uint64_t percentile(double p) const;
    double   average() const;

private:
    std::array<uint64_t, kBucketCount> buckets_;
    uint64_t sum_ = 0;
    uint64_t min_ = UINT64_MAX;
    uint64_t max_ = 0;
    uint64_t count_ = 0;
};

}
