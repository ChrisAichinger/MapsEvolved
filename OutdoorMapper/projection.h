#ifndef ODM__PROJECTION_H
#define ODM__PROJECTION_H

typedef void* projPJ;

class Projection {
    public:
        explicit Projection(const std::string &proj_str);
        ~Projection();

        void PCSToLatLong(double &x, double &y) const;
        void LatLongToPCS(double &x, double &y) const;
        const std::string &GetProjString() const { return m_proj_str; };

    private:
        projPJ m_proj;
        std::string m_proj_str;
};


#endif
