
#include "tiles.h"
#include "rastermap.h"

std::shared_ptr<unsigned int> TileCode::GetTile() const {
    return m_map.GetRegion(m_pos, MapPixelDeltaInt(m_tilesize, m_tilesize));
}
