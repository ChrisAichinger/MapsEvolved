
#include "tiles.h"
#include "rastermap.h"

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