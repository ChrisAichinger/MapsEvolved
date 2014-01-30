#ifndef ODM__TILES_H
#define ODM__TILES_H

#include <memory>

#include "coordinates.h"

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
                     const TileCode& tilecode,
                     const double transparency = 0)
            : m_topleft  (pos.x         , pos.y),
              m_topright (pos.x + size.x, pos.y),
              m_botleft  (pos.x         , pos.y + size.y),
              m_botright (pos.x + size.x, pos.y + size.y),
              m_tilecode(tilecode), m_transparency(transparency)
            {};
        DisplayOrder(const DisplayCoordCentered &TopLeft,
                     const DisplayCoordCentered &TopRight,
                     const DisplayCoordCentered &BotLeft,
                     const DisplayCoordCentered &BotRight,
                     const TileCode& tilecode,
                     const double transparency = 0)
            : m_topleft(TopLeft), m_topright(TopRight),
              m_botleft(BotLeft), m_botright(BotRight),
              m_tilecode(tilecode), m_transparency(transparency)
            {};
        const DisplayCoordCentered &GetTopLeft()  const { return m_topleft; };
        const DisplayCoordCentered &GetTopRight() const { return m_topright; };
        const DisplayCoordCentered &GetBotLeft()  const { return m_botleft; };
        const DisplayCoordCentered &GetBotRight() const { return m_botright; };
        const TileCode &GetTileCode() const { return m_tilecode; };
        double GetTransparency() const { return m_transparency; };

    private:
        DisplayCoordCentered m_topleft;
        DisplayCoordCentered m_topright;
        DisplayCoordCentered m_botleft;
        DisplayCoordCentered m_botright;
        const TileCode m_tilecode;
        double m_transparency;
};

#endif
