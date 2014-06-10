
#include "tiles.h"
#include "rastermap.h"

PixelBuf TileCode::GetTile() const {
    return m_map->GetRegion(m_pos, m_tilesize);
}

PixelBuf DisplayOrderDirect::GetPixels() const {
    return m_map->GetRegionDirect(
            m_size, *m_base_map,
            m_base_pixel_tl, m_base_pixel_br);

}
ODMPixelFormat DisplayOrderTiled::GetPixelFormat() const {
    return m_tilecode.GetMap()->GetPixelFormat();
}
ODMPixelFormat DisplayOrderDirect::GetPixelFormat() const {
    return m_map->GetPixelFormat();
}