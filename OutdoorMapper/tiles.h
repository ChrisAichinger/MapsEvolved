#ifndef ODM__TILES_H
#define ODM__TILES_H

#include <memory>

#include "coordinates.h"

class TileCode {
    public:
        TileCode(const class RasterMap &map,
                 const class MapPixelCoordInt &pos,
                 const class MapPixelDeltaInt &tilesize)
            : m_map(map), m_pos(pos), m_tilesize(tilesize)
            {};
        const class RasterMap &GetMap() const { return m_map; };
        const MapPixelCoordInt &GetPosition() const { return m_pos; };
        const MapPixelDeltaInt &GetTileSize() const { return m_tilesize; };

        std::shared_ptr<unsigned int> GetTile() const;
    private:
        const RasterMap &m_map;
        MapPixelCoordInt m_pos;
        MapPixelDeltaInt m_tilesize;
};

inline bool operator==(const TileCode& lhs, const TileCode& rhs) {
    return &lhs.GetMap() == &rhs.GetMap() &&
           lhs.GetPosition() == rhs.GetPosition() &&
           lhs.GetTileSize() == rhs.GetTileSize();
}
inline bool operator< (const TileCode& lhs, const TileCode& rhs) {
    const class RasterMap &lmap = lhs.GetMap();
    const class RasterMap &rmap = rhs.GetMap();
    // Compare maps by memory address
    // We should only ever have one instance of class RasterMap per map anyway.
    if (&lmap != &rmap) return &lmap < &rmap;

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
                     const TileCode& tilecode)
            : m_topleft  (pos.x         , pos.y),
              m_topright (pos.x + size.x, pos.y),
              m_botleft  (pos.x         , pos.y + size.y),
              m_botright (pos.x + size.x, pos.y + size.y),
              m_tilecode(tilecode)
            {};
        DisplayOrder(const DisplayCoordCentered &TopLeft,
                     const DisplayCoordCentered &TopRight,
                     const DisplayCoordCentered &BotLeft,
                     const DisplayCoordCentered &BotRight,
                     const TileCode& tilecode)
            : m_topleft(TopLeft), m_topright(TopRight),
              m_botleft(BotLeft), m_botright(BotRight),
              m_tilecode(tilecode)
            {};
        const DisplayCoordCentered &GetTopLeft()  const { return m_topleft; };
        const DisplayCoordCentered &GetTopRight() const { return m_topright; };
        const DisplayCoordCentered &GetBotLeft()  const { return m_botleft; };
        const DisplayCoordCentered &GetBotRight() const { return m_botright; };
        const TileCode &GetTileCode() const { return m_tilecode; };

    private:
        DisplayCoordCentered m_topleft;
        DisplayCoordCentered m_topright;
        DisplayCoordCentered m_botleft;
        DisplayCoordCentered m_botright;
        const TileCode m_tilecode;
};

#endif
