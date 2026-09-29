#include "slice_cache.h"

#include <iostream>
#include <utility>

SliceCache::SliceCache(std::string filename, tomocam::tiff::TiffMeta meta,
                       size_t budgetBytes, QObject *parent)
    : QObject(parent), filename_(std::move(filename)), meta_(meta),
      budgetBytes_(budgetBytes),
      policy_(meta_.n_pages, size_t(meta_.width) * meta_.height, budgetBytes_),
      worker_([this](std::stop_token st) { workerLoop(st); }) {}

SliceCache::~SliceCache() {
    worker_.request_stop();
    cv_.notify_all();
    // std::jthread's destructor joins automatically.
}

std::set<uint32_t> SliceCache::cachedIndicesLocked() const {
    std::set<uint32_t> result;
    for (const auto &kv : cached_) result.insert(kv.first);
    return result;
}

std::shared_ptr<const std::vector<uint8_t>> SliceCache::getSlice(uint32_t index) {
    std::unique_lock lock(mutex_);
    if (auto it = cached_.find(index); it != cached_.end()) return it->second;
    if (failed_.count(index)) return nullptr;

    urgentRequest_ = index;
    cv_.notify_all();
    cv_.wait(lock, [&] { return cached_.count(index) || failed_.count(index); });

    auto it = cached_.find(index);
    return it != cached_.end() ? it->second : nullptr;
}

void SliceCache::setCurrentIndex(uint32_t index) {
    {
        std::lock_guard lock(mutex_);
        policy_.setCurrentIndex(index);
    }
    cv_.notify_all();
}

void SliceCache::workerLoop(std::stop_token stopToken) {
    tomocam::tiff::TiffSliceReader reader(filename_);

    while (true) {
        uint32_t target;
        {
            std::unique_lock lock(mutex_);
            bool woke = cv_.wait(lock, stopToken, [&] {
                return urgentRequest_.has_value() ||
                       policy_.nextPrefetchTarget(cachedIndicesLocked()).has_value();
            });
            if (!woke) return; // stop requested, predicate never became true

            target = urgentRequest_.has_value()
                         ? *urgentRequest_
                         : *policy_.nextPrefetchTarget(cachedIndicesLocked());
        }

        std::shared_ptr<std::vector<uint8_t>> bytes;
        bool ok = true;
        try {
            bytes = std::make_shared<std::vector<uint8_t>>(
                reader.readSliceNormalized(target));
        } catch (const std::exception &e) {
            std::cerr << "SliceCache: failed to read slice " << target << ": "
                      << e.what() << std::endl;
            ok = false;
        }

        bool wasUrgent = false;
        {
            std::lock_guard lock(mutex_);
            if (ok) {
                cached_[target] = bytes;
                for (uint32_t idx : policy_.indicesToEvict(cachedIndicesLocked()))
                    cached_.erase(idx);
            } else {
                failed_.insert(target);
            }
            if (urgentRequest_ == target) {
                urgentRequest_.reset();
                wasUrgent = true;
            }
        }
        cv_.notify_all(); // wakes getSlice() waiters
        if (ok && !wasUrgent) emit sliceReady(target);
    }
}
