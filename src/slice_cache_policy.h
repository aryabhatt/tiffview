#ifndef SLICE_CACHE_POLICY__H
#define SLICE_CACHE_POLICY__H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <vector>

// Pure decision logic for SliceCache: which slices to evict and which to
// prefetch next, given a byte budget and the current playhead position.
// No threading, Qt, or file I/O -- kept separate so it's directly
// unit-testable.
//
// Eviction policy is "distance from current index", not LRU: for a
// scrolling/scrubbing access pattern, proximity to the current slice
// predicts future need better than recency does (matches prior art in
// napari's chunk cache and Cornerstone.js's stack viewport).
class SliceCachePolicy {
  public:
    SliceCachePolicy(uint32_t pageCount, size_t bytesPerSlice, size_t budgetBytes);

    size_t maxResidentSlices() const { return maxResident_; }

    // Call whenever the displayed/target index changes; updates the
    // inferred scroll direction used to bias eviction and prefetch.
    void setCurrentIndex(uint32_t index);
    uint32_t currentIndex() const { return currentIndex_; }
    int direction() const { return direction_; }

    // Indices that must be evicted right now to respect the byte budget,
    // given the currently cached set. Never includes currentIndex().
    // Evicts the cached index farthest from currentIndex() first.
    std::vector<uint32_t> indicesToEvict(const std::set<uint32_t> &cached) const;

    // Next slice to prefetch, biased in the direction of travel, or
    // nullopt if the prefetch window is already fully cached (the worker
    // should go idle). The window is up to kPrefetchWindow slices ahead of
    // current in direction(), plus a smaller kTrailingWindow behind, to
    // tolerate short reversals without a cache miss.
    std::optional<uint32_t>
    nextPrefetchTarget(const std::set<uint32_t> &cached) const;

    static constexpr uint32_t kPrefetchWindow = 32;
    static constexpr uint32_t kTrailingWindow = 8;

  private:
    uint32_t pageCount_;
    size_t bytesPerSlice_;
    size_t budgetBytes_;
    size_t maxResident_;
    uint32_t currentIndex_ = 0;
    uint32_t lastIndex_ = 0;
    int direction_ = 1; // +1 forward, -1 backward
};

#endif // SLICE_CACHE_POLICY__H
