#ifndef ODM__TILES_H
#define ODM__TILES_H

#include <memory>
#include <functional>

#include "coordinates.h"
#include "util.h"
#include "pixelbuf.h"

class TileCode {
    public:
        TileCode(const std::shared_ptr<class GeoDrawable> &map,
                 const class MapPixelCoordInt &pos,
                 const class MapPixelDeltaInt &tilesize)
            : m_map(map), m_pos(pos), m_tilesize(tilesize)
            {};
        std::shared_ptr<class GeoDrawable> GetMap() const { return m_map; };
        const MapPixelCoordInt &GetPosition() const { return m_pos; };
        const MapPixelDeltaInt &GetTileSize() const { return m_tilesize; };

        PixelBuf GetTile() const;
    private:
        std::shared_ptr<class GeoDrawable> m_map;
        MapPixelCoordInt m_pos;
        MapPixelDeltaInt m_tilesize;
};

inline bool operator==(const TileCode& lhs, const TileCode& rhs) {
    return &lhs.GetMap() == &rhs.GetMap() &&
           lhs.GetPosition() == rhs.GetPosition() &&
           lhs.GetTileSize() == rhs.GetTileSize();
}
inline bool operator< (const TileCode& lhs, const TileCode& rhs) {
    const class GeoDrawable *lmap = lhs.GetMap().get();
    const class GeoDrawable *rmap = rhs.GetMap().get();
    // Compare maps by memory address
    // We should only ever have one instance of GeoDrawable per map anyway.
    if (lmap != rmap) return lmap < rmap;

    const MapPixelCoordInt &lpos = lhs.GetPosition();
    const MapPixelCoordInt &rpos = rhs.GetPosition();
    if (lpos.x != rpos.x) return lpos.x < rpos.x;
    if (lpos.y != rpos.y) return lpos.y < rpos.y;

    const MapPixelDeltaInt &lsize = lhs.GetTileSize();
    const MapPixelDeltaInt &rsize = rhs.GetTileSize();
    if (lsize.x != rsize.x) return lsize.x < rsize.x;
    if (lsize.y != rsize.y) return lsize.y < rsize.y;

    return false;
}
inline bool operator!=(const TileCode& lhs, const TileCode& rhs) {
    return !operator==(lhs,rhs);
}
inline bool operator> (const TileCode& lhs, const TileCode& rhs) {
    return  operator< (rhs,lhs);
}
inline bool operator<=(const TileCode& lhs, const TileCode& rhs) {
    return !operator> (lhs,rhs);
}
inline bool operator>=(const TileCode& lhs, const TileCode& rhs) {
    return !operator< (lhs,rhs);
}

/**
 An interface to retrieve a PixelBuf at a later time.
*/
class PixelPromise {
    public:
        virtual ~PixelPromise() {};
        /** Retrieve the promised PixelBuf. */
        virtual PixelBuf GetPixels() const = 0;
        /** Get the pixel format of the promised PixelBuf. */
        virtual ODMPixelFormat GetPixelFormat() const = 0;
        /** Get a cache key for possible caching of the delivered PixelBuf.
            This may be nullptr if caching is not supported for the underlying
            drawable.
        */
        virtual const TileCode *GetCacheKey() const = 0;
};

/**
 A PixelPromise implementation supporting NonDirectDraw maps.
*/
class PixelPromiseTiled : public PixelPromise {
    public:
        PixelPromiseTiled(const TileCode& tilecode)
            : PixelPromise(), m_tilecode(tilecode) {};
        virtual ~PixelPromiseTiled() {};
        virtual PixelBuf GetPixels() const {
            return m_tilecode.GetTile();
        }
        virtual ODMPixelFormat GetPixelFormat() const;
        virtual const TileCode *GetCacheKey() const { return &m_tilecode; };
    private:
        const TileCode m_tilecode;
};

/**
 A PixelPromise class getting data from NonDirectDraw maps asynchronously.
*/
class PixelPromiseTiledAsync : public PixelPromise {
    public:
        /** Initialize instance with the `TileCode` to retrieve and a callback.
         *
         * The `TileCode` is resolved to a `PixelBuf` on a background thread,
         * in the meantime, an empty `PixelBuf` is returned, if requested.
         *
         * On completion, the `refresh` callback is called **from the
         * background thread**. In other words, modifying application state
         * from the callback requires special care.
         */
        PixelPromiseTiledAsync(const TileCode& tilecode,
                               const std::function<void()>& refresh);
        virtual ~PixelPromiseTiledAsync();

        /** Get the data if available, otherwise return an empty `PixelBuf` */
        virtual PixelBuf GetPixels() const;

        virtual ODMPixelFormat GetPixelFormat() const;

        /** Get the cache key for this PixelPromise.
         *
         * A valid `TileCode` will only be returned once the pixel data is
         * available. This is to prevent accidental caching of the empty
         * `PixelBuf` returned until the data is ready.
         */
        virtual const TileCode *GetCacheKey() const;
    private:
        DISALLOW_COPY_AND_ASSIGN(PixelPromiseTiledAsync);

        class AsyncWorker;

        TileCode m_tilecode;
        std::shared_ptr<AsyncWorker> m_worker;
};

class PixelPromiseDirect : public PixelPromise {
    public:
        PixelPromiseDirect(
                const std::shared_ptr<class GeoDrawable> &map,
                const MapPixelDeltaInt &size,
                const std::shared_ptr<class GeoPixels> &base_map,
                const MapPixelCoord &base_pixel_tl,
                const MapPixelCoord &base_pixel_br)
            : PixelPromise(), m_map(map), m_size(size), m_base_map(base_map),
              m_base_pixel_tl(base_pixel_tl), m_base_pixel_br(base_pixel_br)
            {};
        virtual ~PixelPromiseDirect() {};

        virtual PixelBuf GetPixels() const;
        virtual ODMPixelFormat GetPixelFormat() const;
        virtual const TileCode *GetCacheKey() const { return nullptr; };
    private:
        const std::shared_ptr<class GeoDrawable> m_map;
        const MapPixelDeltaInt m_size;
        const std::shared_ptr<class GeoPixels> m_base_map;
        const MapPixelCoord m_base_pixel_tl;
        const MapPixelCoord m_base_pixel_br;
};


class DisplayOrder {
    public:
        DisplayOrder(const DisplayRectCentered &rect, double transparency,
                     std::shared_ptr<const PixelPromise> promise)
            : m_rect(rect), m_transparency(transparency), m_promise(promise)
            {};

        const DisplayRectCentered &GetDisplayRect() const {
            return m_rect;
        }
        double GetTransparency() const { return m_transparency; };
        const PixelPromise &GetPixelBufPromise() const {
            return *m_promise;
        }
    private:
        const DisplayRectCentered m_rect;
        const double m_transparency;
        const std::shared_ptr<const PixelPromise> m_promise;
};

#endif
