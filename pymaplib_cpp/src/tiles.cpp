#include "tiles.h"

#include "rastermap.h"
#include "threading.h"

PixelBuf TileCode::GetTile() const {
    return m_map->GetRegion(m_pos, m_tilesize);
}

ODMPixelFormat PixelPromiseTiled::GetPixelFormat() const {
    return m_tilecode.GetMap()->GetPixelFormat();
}

PixelBuf PixelPromiseDirect::GetPixels() const {
    return m_map->GetRegionDirect(
            m_size, *m_base_map, m_base_pixel_tl, m_base_pixel_br);
}
ODMPixelFormat PixelPromiseDirect::GetPixelFormat() const {
    return m_map->GetPixelFormat();
}


/** A local task group runner supporting PixelPromiseTiledAsync */
static ThreadedTaskGroupRunner TGRunner;

/** A thread-safe callable for retrieving PixelBufs from NonDirectDraw maps.
 *
 * This is a helper class for PixelPromiseTiledAsync. On creation, it is
 * supplied with a `TileCode` argument. When the class is called, it obtains
 * image data from the `TileCode`. Thereafter, `GetPixels()` can be used to
 * access the pixel data.
 *
 * All public methods are safe for use from multiple threads. Especially,
 * `operator()` can be running in a background thread, while an UI thread
 * queries the completion status (`IsDone()`) on an UI thread.
 *
 * Note that concurrent access to the underlaying `GeoDrawable` may still cause
 * problems, though.
 *
 * @locking No explicit locking is performed, synchronization is ensured
 * by use of atomic types.
 */
class PixelPromiseTiledAsync::AsyncWorker {
public:
    /** Initialize instance with the `TileCode` to retrieve. */
    AsyncWorker(const TileCode& tilecode)
        : m_tilecode(tilecode), m_pixels() , m_done(false), m_abort(false),
          m_already_called(false)
    { }

    /** Resolve the `TileCode` to an actual `PixelBuf`.
     *
     * This function must not be called more than once.
     */
    void operator()() {
        assert(m_already_called.exchange(true) == false);
        if (!m_abort) {
            m_pixels = m_tilecode.GetTile();
            m_done = true;
        }
    }

    /** Get the desired `PixelBuf`.
     *
     * If it is not yet available (i.e. `operator()` has not yet finished),
     * an empy PixelBuf object is returned.
     */
    PixelBuf GetPixels() const {
        if (m_done) {
            return m_pixels;
        }
        return PixelBuf();
    }

    /** Return `true` if the desired `PixelBuf` is available yet. */
    bool IsDone() const { return m_done; }

    /** Abort the resolution process, if still possible.
     *
     * This can be used to prevent `PixelBuf` resolution if it has not yet
     * started. It is useful for preventing unnecessary work if an instance
     * has been submitted to a task runner, but the result is not required any
     * more.
     *
     * `Abort()` works on a best-effort basis. If work has already started on
     * a different thread, it will complete.
     */
    void Abort() { m_abort = true; }

private:
    const TileCode m_tilecode;
    PixelBuf m_pixels;

    // m_pixels was retrieved and can be accessed. We're done.
    boost::atomic<bool> m_done;

    // Do not start any new computations, just die quickly.
    boost::atomic<bool> m_abort;

    // For debugging purposes only. Assert we are not called twice.
    boost::atomic<bool> m_already_called;
};


PixelPromiseTiledAsync::PixelPromiseTiledAsync(
        const TileCode& tilecode,
        const std::function<void()>& refresh)
    : PixelPromise(), m_tilecode(tilecode), m_worker(new AsyncWorker(tilecode))
{
    // Use the local, static ThreadedTaskGroupRunner to run a lambda function
    // on a different thread. First resolve the TileCode using the AsyncWorker,
    // then call the refresh function to update the display.
    std::shared_ptr<AsyncWorker> worker = m_worker;
    TGRunner.Enqueue(tilecode.GetMap().get(), [worker, refresh](){
        (*worker)();
        refresh();
    });
}

PixelPromiseTiledAsync::~PixelPromiseTiledAsync() {
    // Do not resolve the TileCode if the work has not yet started.
    // We are not interested in the result any more.
    m_worker->Abort();
}

PixelBuf PixelPromiseTiledAsync::GetPixels() const {
    return m_worker->GetPixels();
}

ODMPixelFormat PixelPromiseTiledAsync::GetPixelFormat() const {
    return m_tilecode.GetMap()->GetPixelFormat();
}

const TileCode* PixelPromiseTiledAsync::GetCacheKey() const {
    // Enable caching only once the pixels are available.
    if (m_worker->IsDone()) {
        return &m_tilecode;
    } else {
        return nullptr;
    }
}
