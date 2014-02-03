#ifndef ODM__TILES_H
#define ODM__TILES_H

#include <memory>

#include "coordinates.h"
#include "util.h"

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

        std::shared_ptr<unsigned int> GetTile() const;
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


class DisplayOrder {
    public:
        DisplayOrder(const DisplayCoordCentered &pos, const DisplayDelta &size,
                     const double transparency = 0)
            : m_topleft  (pos.x,          pos.y),
              m_topright (pos.x + size.x, pos.y),
              m_botleft  (pos.x,          pos.y + size.y),
              m_botright (pos.x + size.x, pos.y + size.y),
              m_transparency(transparency)
            {};
        DisplayOrder(const DisplayCoordCentered &TopLeft,
                     const DisplayCoordCentered &TopRight,
                     const DisplayCoordCentered &BotLeft,
                     const DisplayCoordCentered &BotRight,
                     const double transparency = 0)
            : m_topleft(TopLeft), m_topright(TopRight),
              m_botleft(BotLeft), m_botright(BotRight),
              m_transparency(transparency)
            {};
        virtual ~DisplayOrder() {};

        virtual const DisplayCoordCentered &GetTopLeft()  const {
            return m_topleft;
        };
        virtual const DisplayCoordCentered &GetTopRight() const {
            return m_topright;
        };
        virtual const DisplayCoordCentered &GetBotLeft()  const {
            return m_botleft;
        };
        virtual const DisplayCoordCentered &GetBotRight() const {
            return m_botright;
        };
        virtual double GetTransparency() const { return m_transparency; };
        virtual std::shared_ptr<unsigned int> GetPixels() const = 0;
        virtual MapPixelDeltaInt GetPixelSize() const = 0;

        virtual bool IsCachable() const { return !!GetTileCode(); };
        virtual const TileCode *GetTileCode() const = 0;
    protected:
        DisplayCoordCentered m_topleft;
        DisplayCoordCentered m_topright;
        DisplayCoordCentered m_botleft;
        DisplayCoordCentered m_botright;
        double m_transparency;
};


class DisplayOrderTiled : public DisplayOrder {
    public:
        DisplayOrderTiled(const DisplayCoordCentered &pos,
                          const DisplayDelta &size,
                          const TileCode& tilecode,
                          const double transparency)
            : DisplayOrder(pos, size, transparency), m_tilecode(tilecode)
            {};
        DisplayOrderTiled(const DisplayCoordCentered &TopLeft,
                          const DisplayCoordCentered &TopRight,
                          const DisplayCoordCentered &BotLeft,
                          const DisplayCoordCentered &BotRight,
                          const TileCode& tilecode,
                          const double transparency)
            : DisplayOrder(TopLeft, TopRight, BotLeft, BotRight),
              m_tilecode(tilecode)
            {};
        virtual ~DisplayOrderTiled() {};
        const TileCode *GetTileCode() const { return &m_tilecode; };
        virtual std::shared_ptr<unsigned int> GetPixels() const {
            return m_tilecode.GetTile();
        }
        virtual MapPixelDeltaInt GetPixelSize() const {
            return m_tilecode.GetTileSize();
        }

    private:
        const TileCode m_tilecode;
};

class DisplayOrderDirect : public DisplayOrder {
    public:
        DisplayOrderDirect(
                const std::shared_ptr<class GeoDrawable> &map,
                const MapPixelDeltaInt &size,
                const std::shared_ptr<class GeoPixels> &base_map,
                const MapPixelCoord &base_pixel_tl,
                const MapPixelCoord &base_pixel_br,
                const double transparency)
            : DisplayOrder(DisplayCoordCentered(-size.x/2.0, -size.y/2.0),
                           DisplayDelta(size.x, size.y), transparency),
              m_map(map), m_size(size), m_base_map(base_map),
              m_base_pixel_tl(base_pixel_tl), m_base_pixel_br(base_pixel_br)
            {};
        virtual ~DisplayOrderDirect() {};
        virtual const TileCode *GetTileCode() const { return nullptr; };
        virtual std::shared_ptr<unsigned int> GetPixels() const;
        virtual MapPixelDeltaInt GetPixelSize() const {
            return MapPixelDeltaInt(round_to_int(m_size.x),
                                    round_to_int(m_size.y));
        }

    private:
        const std::shared_ptr<class GeoDrawable> m_map;
        const MapPixelDeltaInt m_size;
        const std::shared_ptr<class GeoPixels> m_base_map;
        const MapPixelCoord m_base_pixel_tl;
        const MapPixelCoord m_base_pixel_br;
};

#endif
