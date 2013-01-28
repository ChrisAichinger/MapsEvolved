#ifndef ODM__TILES_H
#define ODM__TILES_H

#include <memory>

class TileCode {
    public:
        TileCode(const class RasterMap &map, int x, int y, int tilesize)
            : m_map(map), m_x(x), m_y(y), m_tilesize(tilesize)
            {};
        const class RasterMap &GetMap() const { return m_map; };
        int GetZoomIndex() const { return m_zoomIndex; };
        int GetX() const { return m_x; };
        int GetY() const { return m_y; };
        int GetTileSize() const { return m_tilesize; };

        std::shared_ptr<unsigned int> GetTile() const;
    private:
        const RasterMap &m_map;
        int m_zoomIndex;
        int m_x, m_y;
        int m_tilesize;
};

inline bool operator==(const TileCode& lhs, const TileCode& rhs) {
    return &lhs.GetMap() == &rhs.GetMap() &&
           lhs.GetZoomIndex() == rhs.GetZoomIndex() &&
           lhs.GetX() == rhs.GetX() &&
           lhs.GetY() == rhs.GetY() &&
           lhs.GetTileSize() == rhs.GetTileSize();
}
inline bool operator< (const TileCode& lhs, const TileCode& rhs) {
    const class RasterMap &lmap = lhs.GetMap();
    const class RasterMap &rmap = rhs.GetMap();
    // Compare maps by memory address
    // We should only ever have one instance of class RasterMap per map anyway.
    if (&lmap != &rmap) return &lmap < &rmap;

    int lzoomidx = lhs.GetZoomIndex();
    int rzoomidx = rhs.GetZoomIndex();
    if (lzoomidx != rzoomidx) return lzoomidx < rzoomidx;

    int lx = lhs.GetX();
    int rx = rhs.GetX();
    if (lx != rx) return lx < rx;

    int ly = lhs.GetY();
    int ry = rhs.GetY();
    if (ly != ry) return ly < ry;

    return lhs.GetTileSize() < rhs.GetTileSize();
}
inline bool operator!=(const TileCode& lhs, const TileCode& rhs) {return !operator==(lhs,rhs);}
inline bool operator> (const TileCode& lhs, const TileCode& rhs) {return  operator< (rhs,lhs);}
inline bool operator<=(const TileCode& lhs, const TileCode& rhs) {return !operator> (lhs,rhs);}
inline bool operator>=(const TileCode& lhs, const TileCode& rhs) {return !operator< (lhs,rhs);}


class DisplayOrder {
    public:
        DisplayOrder(double x, double y, double width, double height,
                     const TileCode& tilecode)
            : m_x(x), m_y(y), m_width(width), m_height(height),
              m_tilecode(tilecode)
            {};
        double GetX() const { return m_x; };
        double GetY() const { return m_y; };
        double GetWidth() const { return m_width; };
        double GetHeight() const { return m_height; };
        const TileCode &GetTileCode() const { return m_tilecode; };

    private:
        double m_x, m_y, m_width, m_height;
        const TileCode m_tilecode;
};

#endif
