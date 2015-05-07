#include "map_composite.h"

#include <regex>
#include <sstream>
#include <type_traits>

#include "util.h"

CompositeMap::CompositeMap(
       unsigned int num_x,
       unsigned int num_y,
       bool has_overlap_pixel,
       const std::vector<std::shared_ptr<RasterMap> > &submaps)
    : m_num_x(num_x), m_num_y(num_y),
      m_overlap_pixel(has_overlap_pixel ? 1 : 0),
      m_submaps(submaps), m_width(0), m_height(0), m_fname(), m_title(),
      m_description(), m_concurrent_getregion(true)
{
    init();
}

CompositeMap::CompositeMap(const std::wstring &fname_token)
    : m_num_x(0), m_num_y(0), m_overlap_pixel(0), m_submaps(),
      m_width(0), m_height(0), m_fname(), m_title(), m_description(),
      m_concurrent_getregion(true)
{
    bool has_overlap_pixel;
    m_submaps = LoadFnameMaps(fname_token, &m_num_x, &m_num_y,
                              &has_overlap_pixel);
    m_overlap_pixel = (has_overlap_pixel ? 1 : 0);
    init();
}

void CompositeMap::init() {
    if (m_num_x * m_num_y != m_submaps.size()) {
        throw std::runtime_error("Size does not match number of passed maps");
    }
    if (m_num_x < 1 || m_num_y < 1) {
        throw std::runtime_error("Can not generate empty combined map");
    }
    for (unsigned int x = 0; x < m_num_x; ++x) {
        m_width += SubmapWidth(x, 0);
    }
    for (unsigned int y = 0; y < m_num_y; ++y) {
        m_height += SubmapHeight(0, y);
    }
    m_fname = FormatFname(*this);
    m_title = L"Composite map";

    std::wostringstream ss;
    ss << L"Submaps:\n";
    for (auto it = m_submaps.cbegin(); it != m_submaps.cend(); ++it) {
        ss << (*it)->GetTitle() << L"\n";
    }
    m_description = ss.str();

    for (auto it = m_submaps.cbegin(); it != m_submaps.cend(); ++it) {
        m_concurrent_getregion &= (*it)->SupportsConcurrentGetRegion();
    }
}

template <typename T>
std::tuple<unsigned int, unsigned int, T>
CompositeMap::find_submap(T coord) const {
    static_assert(std::is_same<T, MapPixelCoord>::value ||
                  std::is_same<T, MapPixelCoordInt>::value,
                  "find_submap() only supports MapPixelCoord and "
                  "MapPixelCoordInt as argument types.");

    // Do not iterate over the last submap:
    // If we get coord.x > m_width, we return the last submap with
    // coord.x > submap.GetWidth(). Same consideration for the y axis.
    unsigned int x, y;
    for (x = 0; x < m_num_x - 1; ++x) {
        unsigned int map_width = SubmapWidth(x, 0);
        if (coord.x < static_cast<int>(map_width)) {
            break;
        }
        coord.x -= map_width;
    }
    for (y = 0; y < m_num_x - 1; ++y) {
        unsigned int map_height = SubmapHeight(0, y);
        if (coord.y < static_cast<int>(map_height)) {
            break;
        }
        coord.y -= map_height;
    }
    return std::make_tuple(x, y, coord);
}

unsigned int
CompositeMap::SubmapWidth(unsigned int mx, unsigned int my) const {
    return m_submaps[index(mx, my)]->GetWidth() - m_overlap_pixel;
}
unsigned int
CompositeMap::SubmapHeight(unsigned int mx, unsigned int my) const {
    return m_submaps[index(mx, my)]->GetHeight() - m_overlap_pixel;
}

GeoDrawable::DrawableType CompositeMap::GetType() const {
    return m_submaps[0]->GetType();
}
unsigned int CompositeMap::GetWidth() const {
    return m_width;
}
unsigned int CompositeMap::GetHeight() const {
    return m_height;
}
MapPixelDeltaInt CompositeMap::GetSize() const {
    return MapPixelDeltaInt(m_width, m_height);
}
Projection CompositeMap::GetProj() const {
    return m_submaps[0]->GetProj();
}

bool
CompositeMap::PixelToLatLon(const MapPixelCoord &pos, LatLon *result) const
{
    unsigned int x, y;
    MapPixelCoord new_coord;
    std::tie(x, y, new_coord) = find_submap(pos);
    return m_submaps[index(x, y)]->PixelToLatLon(new_coord, result);
}

bool
CompositeMap::LatLonToPixel(const LatLon &pos, MapPixelCoord *output) const {
    MapPixelCoord res;
    unsigned int x_offset = 0;
    for (unsigned int x = 0; x < m_num_x; ++x) {
        unsigned int cur_map_width = SubmapWidth(x, 0);
        unsigned int y_offset = 0;
        for (unsigned int y = 0; y < m_num_y; ++y) {
            auto cur_map = m_submaps[index(x, y)];
            unsigned int cur_map_height = SubmapHeight(x, y);
            if (!cur_map->LatLonToPixel(pos, &res)) {
                return false;
            }
            if (res.x >= 0 && res.x <= cur_map_width &&
                res.y >= 0 && res.y <= cur_map_height)
            {
                res.x += x_offset;
                res.y += y_offset;
                *output = res;
                return true;
            }
            y_offset += cur_map_height;
        }
        x_offset += cur_map_width;
    }
    return false;
}

const std::wstring &CompositeMap::GetFname() const {
    return m_fname;
}

const std::wstring &CompositeMap::GetTitle() const {
    return m_title;
}

const std::wstring &CompositeMap::GetDescription() const {
    return m_description;
}

bool CompositeMap::SupportsConcurrentGetRegion() const {
    // CompositeMap can safely be used from threads if each of the submaps can.
    for (auto it=m_submaps.cbegin(); it != m_submaps.cend(); ++it) {
        if (!(*it)->SupportsConcurrentGetRegion()) {
            return false;
        }
    }
    return true;
}

PixelBuf CompositeMap::GetRegion(
        const MapPixelCoordInt &pos, const MapPixelDeltaInt &size) const
{
    auto fixed_bounds_pb = GetRegion_BoundsHelper(*this, pos, size);
    if (fixed_bounds_pb.GetData())
        return fixed_bounds_pb;

    unsigned int tl_x, tl_y, br_x, br_y;
    MapPixelCoordInt tl_pos, br_pos;

    // GetRegion() size is not inclusive: the last pixel we retrieve is
    // pos + size - one_px. Compensate for that in our submap lookup.
    MapPixelDeltaInt one_px(1, 1);
    std::tie(tl_x, tl_y, tl_pos) = find_submap(pos);
    std::tie(br_x, br_y, br_pos) = find_submap(pos + size - one_px);
    br_pos += one_px;

    if (tl_x == br_x && tl_y == br_y) {
        // Fast-path, the requested region spans only one submap.
        // Avoid pointlessly copying the buffer.
        auto map = m_submaps[index(tl_x, tl_y)];
        return  map->GetRegion(tl_pos, br_pos - tl_pos);
    }

    PixelBuf result(size.x, size.y);
    MapPixelDeltaInt subregion_sz;
    unsigned int x_offset = 0;
    for (unsigned int x = tl_x; x <= br_x; ++x) {
        unsigned int y_offset = 0;
        for (unsigned int y = tl_y; y <= br_y; ++y) {
            auto map = m_submaps[index(x, y)];

            // x, y, tl_x, tl_y, br_x, br_y: submap indices
            // [xy]_start, [xy]_end: pixel coords in submap
            // [xy]_offset: position of subregion within result region

            int x_start = (x == tl_x) ? tl_pos.x : 0;
            int y_start = (y == tl_y) ? tl_pos.y : 0;
            int x_end =   (x == br_x) ? br_pos.x : SubmapWidth(x, y);
            int y_end =   (y == br_y) ? br_pos.y : SubmapHeight(x, y);

            auto subregion_pos = MapPixelCoordInt(x_start, y_start);
            subregion_sz = MapPixelDeltaInt(x_end - x_start, y_end - y_start);
            PixelBuf subregion = map->GetRegion(subregion_pos, subregion_sz);
            // BitBlt subregion into our result region at [xy]_offset.
            PixelBufCoord target(x_offset, size.y - subregion_sz.y - y_offset);
            result.Insert(target, subregion);

            y_offset += subregion_sz.y;
        }
        x_offset += subregion_sz.x;
    }
    return result;
}

typedef std::regex_iterator<std::wstring::iterator> wstring_regex_it;

std::vector<std::wstring>
CompositeMap::ParseFname(const std::wstring &fname,
                         unsigned int *num_maps_x,
                         unsigned int *num_maps_y,
                         bool *has_overlap_pixel)
{
    std::wregex re(L"composite_map:(\\d+);(\\d+);(clip|noclip);(.*)");
    std::wsmatch m;
    std::regex_match(fname, m, re);
    *num_maps_x = std::stoul(m[1]);
    *num_maps_y = std::stoul(m[2]);
    *has_overlap_pixel = (m[3] == L"clip");
    std::wstring fnames_str(m[4]);

    std::vector<std::wstring> fnames;
    re = L"([^;]+)(?:;|$)";
    wstring_regex_it rit (fnames_str.begin(), fnames_str.end(), re);
    wstring_regex_it rend;
    for (; rit != rend; ++rit) {
        fnames.push_back(url_decode(rit->operator[](1)));
    }
    return fnames;
}

std::wstring
CompositeMap::FormatFname(unsigned int num_maps_x,
                          unsigned int num_maps_y,
                          bool has_overlap_pixel,
                          const std::vector<std::wstring> &fnames)
{
    std::wostringstream ss;
    ss << L"composite_map:" << num_maps_x << L";" << num_maps_y << L";";
    ss << (has_overlap_pixel ? L"clip" : L"noclip") << L";";
    for (auto it = fnames.cbegin(); it != fnames.cend(); ++it) {
        ss << url_encode(*it) << L";";
    }
    return ss.str();
}

std::vector<std::shared_ptr<RasterMap>>
CompositeMap::LoadFnameMaps(const std::wstring &fname,
                            unsigned int *num_maps_x,
                            unsigned int *num_maps_y,
                            bool *has_overlap_pixel)
{
    auto fnames = ParseFname(fname, num_maps_x, num_maps_y, has_overlap_pixel);
    std::vector<std::shared_ptr<RasterMap>> maps;
    for (auto it = fnames.cbegin(); it != fnames.cend(); ++it) {
        maps.push_back(LoadMap(*it));
    }
    return maps;
}

std::wstring
CompositeMap::FormatFname(
    unsigned int num_maps_x, unsigned int num_maps_y,
    bool has_overlap_pixel,
    const std::vector<std::shared_ptr<RasterMap>> &submaps)
{
    std::vector<std::wstring> fnames;
    for (auto it = submaps.cbegin(); it != submaps.cend(); ++it) {
        fnames.push_back((*it)->GetFname());
    }
    return FormatFname(num_maps_x, num_maps_y, has_overlap_pixel, fnames);
}

std::wstring CompositeMap::FormatFname(const CompositeMap &this_map) {
    return FormatFname(this_map.m_num_x, this_map.m_num_y,
                       this_map.m_overlap_pixel != 0, this_map.m_submaps);
}
