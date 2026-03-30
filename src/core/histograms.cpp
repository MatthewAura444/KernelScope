/*
 * histograms.cpp — Log2 histogram for KernelScope.
 *
 * Fixed-bucket histogram using log2 indexing. Records nanosecond
 * values and provides percentile estimation with bucket midpoint
 * interpolation.
 */

#include "core/histograms.hpp"

#include <algorithm>
#include <bit>

namespace kscope {

Histogram::Histogram() {
    buckets_.fill(0);
}

void Histogram::record(uint64_t value_ns) {
    size_t idx = (value_ns == 0) ? 0 :
        std::min(static_cast<size_t>(std::bit_width(value_ns) - 1),
                 kBucketCount - 1);
    buckets_[idx]++;
    sum_ += value_ns;
    count_++;
    if (value_ns < min_) min_ = value_ns;
    if (value_ns > max_) max_ = value_ns;
}

void Histogram::reset() {
    buckets_.fill(0);
    sum_ = 0;
    min_ = UINT64_MAX;
    max_ = 0;
    count_ = 0;
}

uint64_t Histogram::count() const {
    return count_;
}

double Histogram::average() const {
    if (count_ == 0) return 0.0;
    return static_cast<double>(sum_) / static_cast<double>(count_);
}

uint64_t Histogram::percentile(double p) const {
    if (count_ == 0) return 0;

    uint64_t threshold = static_cast<uint64_t>(
        static_cast<double>(count_) * p / 100.0);
    uint64_t cumulative = 0;

    for (size_t i = 0; i < kBucketCount; i++) {
        cumulative += buckets_[i];
        if (cumulative >= threshold) {
            uint64_t lo = 1ULL << i;
            uint64_t hi = (i + 1 < 64) ? (1ULL << (i + 1)) : lo;
            return lo + (hi - lo) / 2;
        }
    }
    return max_;
}

}
