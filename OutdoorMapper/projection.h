#ifndef ODM__PROJECTION_H
#define ODM__PROJECTION_H

#include <memory>

#include "util.h"

class EXPORT Projection {
    public:
        explicit Projection(const std::string &proj_str);

        bool PCSToLatLong(double &x, double &y) const;
        bool LatLongToPCS(double &x, double &y) const;
        const std::string &GetProjString() const { return m_proj_str; };

        bool CalcDistance(double lat1, double long1,
                          double lat2, double long2,
                          double *distance) const;

        bool IsValid() const { return m_is_valid; }

    private:
        std::shared_ptr<class ProjWrap> m_proj;
        std::string m_proj_str;
        bool m_is_valid;
};


#endif
