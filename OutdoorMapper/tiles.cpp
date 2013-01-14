
#include "tiles.h"
#include "rastermap.h"

std::shared_ptr<unsigned int> TileCode::GetTile() const {
    return m_map.GetRegion(m_x, m_y, m_tilesize, m_tilesize);
}
