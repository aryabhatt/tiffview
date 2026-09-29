#ifndef SLICE_CACHE__H
#define SLICE_CACHE__H

#include <QObject>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "io/tiff/tiff_reader.h"
#include "slice_cache_policy.h"

// Bounded-memory cache of per-slice-normalized (uint8) Z-axis TIFF pages.
// A single dedicated background std::jthread owns the only open
// TiffSliceReader for the file's lifetime and serially services prefetch
// requests (direction-aware) and urgent synchronous misses. Public
// methods are safe to call from the UI thread.
class SliceCache : public QObject {
    Q_OBJECT

  public:
    // Probes `filename` for `meta` are expected to already be known by
    // the caller (via tomocam::tiff::probe); throws std::runtime_error if
    // the worker fails to open the file.
    SliceCache(std::string filename, tomocam::tiff::TiffMeta meta,
               size_t budgetBytes, QObject *parent = nullptr);
    ~SliceCache() override;

    SliceCache(const SliceCache &) = delete;
    SliceCache &operator=(const SliceCache &) = delete;

    // Synchronous "get me this slice now" call for the UI thread. Returns
    // the cached bytes immediately on a hit. On a miss, issues an urgent
    // request to the worker and blocks until it completes, so the UI
    // never shows blank/stale content. Returns nullptr if the read
    // failed (caller should show a placeholder; the failure is already
    // logged to stderr).
    std::shared_ptr<const std::vector<uint8_t>> getSlice(uint32_t index);

    // Updates the playhead position (and inferred direction), re-evaluates
    // eviction/prefetch, and wakes the worker. Cheap; call on every
    // navigation event, before calling getSlice for the new index.
    void setCurrentIndex(uint32_t index);

    uint32_t pageCount() const { return meta_.n_pages; }
    uint32_t width() const { return meta_.width; }
    uint32_t height() const { return meta_.height; }
    size_t bytesPerSlice() const { return size_t(meta_.width) * meta_.height; }

  signals:
    // Emitted from the worker thread when a background (non-urgent)
    // prefetch completes; always delivered via Qt::QueuedConnection so
    // slots run on the receiver's (UI) thread. ImageViewer connects this
    // and repaints only if `index` is the slice currently on screen.
    void sliceReady(uint32_t index);

  private:
    void workerLoop(std::stop_token stopToken);
    std::set<uint32_t> cachedIndicesLocked() const; // requires mutex_ held

    std::string filename_;
    tomocam::tiff::TiffMeta meta_;
    size_t budgetBytes_;

    mutable std::mutex mutex_;
    std::condition_variable_any cv_;
    std::map<uint32_t, std::shared_ptr<std::vector<uint8_t>>>
        cached_;                            // guarded by mutex_
    SliceCachePolicy policy_;               // guarded by mutex_
    std::optional<uint32_t> urgentRequest_; // guarded by mutex_
    std::set<uint32_t> failed_;             // guarded by mutex_

    std::jthread worker_; // must be declared/initialized last
};

#endif // SLICE_CACHE__H
