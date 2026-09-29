#include "slice_cache_policy.h"

#include <algorithm>

SliceCachePolicy::SliceCachePolicy(uint32_t pageCount, size_t bytesPerSlice,
                                   size_t budgetBytes)
    : pageCount_(pageCount), bytesPerSlice_(bytesPerSlice),
      budgetBytes_(budgetBytes) {
    size_t byBudget =
        (bytesPerSlice_ > 0) ? budgetBytes_ / bytesPerSlice_ : size_t(pageCount_);
    maxResident_ = std::max<size_t>(1, byBudget);
}

void SliceCachePolicy::setCurrentIndex(uint32_t index) {
    if (index == currentIndex_) return;

    int64_t diff = int64_t(index) - int64_t(currentIndex_);
    int64_t half = int64_t(pageCount_) / 2;
    if (half > 0 && (diff > half || diff < -half)) {
        // Jump larger than half the stack (e.g. Home/End): treat as a
        // non-directional wrap and keep the previously inferred direction.
    } else {
        direction_ = (diff > 0) ? 1 : -1;
    }

    lastIndex_ = currentIndex_;
    currentIndex_ = index;
}

std::vector<uint32_t>
SliceCachePolicy::indicesToEvict(const std::set<uint32_t> &cached) const {
    if (cached.size() <= maxResident_) return {};

    std::vector<uint32_t> candidates;
    candidates.reserve(cached.size());
    for (uint32_t idx : cached)
        if (idx != currentIndex_) candidates.push_back(idx);

    auto distance = [&](uint32_t idx) {
        return idx > currentIndex_ ? idx - currentIndex_ : currentIndex_ - idx;
    };

    std::sort(candidates.begin(), candidates.end(), [&](uint32_t a, uint32_t b) {
        uint32_t da = distance(a);
        uint32_t db = distance(b);
        if (da != db) return da > db; // farthest first

        // tie-break: evict the one behind the direction of travel first
        bool aAhead = (direction_ > 0) ? (a > currentIndex_) : (a < currentIndex_);
        bool bAhead = (direction_ > 0) ? (b > currentIndex_) : (b < currentIndex_);
        if (aAhead != bAhead) return !aAhead;
        return a < b;
    });

    size_t evictCount = std::min(candidates.size(), cached.size() - maxResident_);
    candidates.resize(evictCount);
    return candidates;
}

std::optional<uint32_t>
SliceCachePolicy::nextPrefetchTarget(const std::set<uint32_t> &cached) const {
    for (uint32_t step = 1; step <= kPrefetchWindow; step++) {
        int64_t idx = int64_t(currentIndex_) + int64_t(direction_) * int64_t(step);
        if (idx < 0 || idx >= int64_t(pageCount_)) continue;
        if (!cached.count(uint32_t(idx))) return uint32_t(idx);
    }
    for (uint32_t step = 1; step <= kTrailingWindow; step++) {
        int64_t idx = int64_t(currentIndex_) - int64_t(direction_) * int64_t(step);
        if (idx < 0 || idx >= int64_t(pageCount_)) continue;
        if (!cached.count(uint32_t(idx))) return uint32_t(idx);
    }
    return std::nullopt;
}
