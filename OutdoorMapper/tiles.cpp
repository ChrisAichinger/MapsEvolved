
#include "tiles.h"
#include "rastermap.h"

std::shared_ptr<unsigned int> TileCode::GetTile() const {
    return m_map->GetRegion(m_pos, m_tilesize).GetData();
}

std::shared_ptr<unsigned int> DisplayOrderDirect::GetPixels() const {
    return m_map->GetRegionDirect(
            m_size, *m_base_map,
            m_base_pixel_tl, m_base_pixel_br).GetData();

}
