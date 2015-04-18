#ifndef ODM__PROJECTION_H
#define ODM__PROJECTION_H

#include <memory>

#include "util.h"

// A projection class backed by proj4. When crafting proj4 initialization
// strings, take care to use the correct spelling (and capitalization) of
// projection, ellipse and datum names.

// Supported projections:
// "aea":       Albers Equal Area
// "aeqd":      Azimuthal Equidistant
// "airy":      Airy
// "aitoff":    Aitoff
// "alsk":      Mod. Stererographics of Alaska
// "apian":     Apian Globular I
// "august":    August Epicycloidal
// "bacon":     Bacon Globular
// "bipc":      Bipolar conic of western hemisphere
// "boggs":     Boggs Eumorphic
// "bonne":     Bonne (Werner lat_1=90)
// "cass":      Cassini
// "cc":        Central Cylindrical
// "cea":       Equal Area Cylindrical
// "chamb":     Chamberlin Trimetric
// "collg":     Collignon
// "crast":     Craster Parabolic (Putnins P4)
// "denoy":     Denoyer Semi-Elliptical
// "eck1":      Eckert I
// "eck2":      Eckert II
// "eck3":      Eckert III
// "eck4":      Eckert IV
// "eck5":      Eckert V
// "eck6":      Eckert VI
// "eqc":       Equidistant Cylindrical (Plate Caree)
// "eqdc":      Equidistant Conic
// "euler":     Euler
// "etmerc":    Extended Transverse Mercator" )
// "fahey":     Fahey
// "fouc":      Foucaut
// "fouc_s":    Foucaut Sinusoidal
// "gall":      Gall (Gall Stereographic)
// "geocent":   Geocentric
// "geos":      Geostationary Satellite View
// "gins8":     Ginsburg VIII (TsNIIGAiK)
// "gn_sinu":   General Sinusoidal Series
// "gnom":      Gnomonic
// "goode":     Goode Homolosine
// "gs48":      Mod. Stererographics of 48 U.S.
// "gs50":      Mod. Stererographics of 50 U.S.
// "hammer":    Hammer & Eckert-Greifendorff
// "hatano":    Hatano Asymmetrical Equal Area
// "healpix":   HEALPix
// "rhealpix":  rHEALPix
// "igh":       Interrupted Goode Homolosine
// "imw_p":     Internation Map of the World Polyconic
// "isea":      Icosahedral Snyder Equal Area
// "kav5":      Kavraisky V
// "kav7":      Kavraisky VII
// "krovak":    Krovak
// "labrd":     Laborde
// "laea":      Lambert Azimuthal Equal Area
// "lagrng":    Lagrange
// "larr":      Larrivee
// "lask":      Laskowski
// "lonlat":    Lat/long (Geodetic)
// "latlon":    Lat/long (Geodetic alias)
// "latlong":   Lat/long (Geodetic alias)
// "longlat":   Lat/long (Geodetic alias)
// "lcc":       Lambert Conformal Conic
// "lcca":      Lambert Conformal Conic Alternative
// "leac":      Lambert Equal Area Conic
// "lee_os":    Lee Oblated Stereographic
// "loxim":     Loximuthal
// "lsat":      Space oblique for LANDSAT
// "mbt_s":     McBryde-Thomas Flat-Polar Sine
// "mbt_fps":   McBryde-Thomas Flat-Pole Sine (No. 2)
// "mbtfpp":    McBride-Thomas Flat-Polar Parabolic
// "mbtfpq":    McBryde-Thomas Flat-Polar Quartic
// "mbtfps":    McBryde-Thomas Flat-Polar Sinusoidal
// "merc":      Mercator
// "mil_os":    Miller Oblated Stereographic
// "mill":      Miller Cylindrical
// "moll":      Mollweide
// "murd1":     Murdoch I
// "murd2":     Murdoch II
// "murd3":     Murdoch III
// "natearth":  Natural Earth
// "nell":      Nell
// "nell_h":    Nell-Hammer
// "nicol":     Nicolosi Globular
// "nsper":     Near-sided perspective
// "nzmg":      New Zealand Map Grid
// "ob_tran":   General Oblique Transformation
// "ocea":      Oblique Cylindrical Equal Area
// "oea":       Oblated Equal Area
// "omerc":     Oblique Mercator
// "ortel":     Ortelius Oval
// "ortho":     Orthographic
// "pconic":    Perspective Conic
// "poly":      Polyconic (American)
// "putp1":     Putnins P1
// "putp2":     Putnins P2
// "putp3":     Putnins P3
// "putp3p":    Putnins P3'
// "putp4p":    Putnins P4'
// "putp5":     Putnins P5
// "putp5p":    Putnins P5'
// "putp6":     Putnins P6
// "putp6p":    Putnins P6'
// "qua_aut":   Quartic Authalic
// "robin":     Robinson
// "rouss":     Roussilhe Stereographic
// "rpoly":     Rectangular Polyconic
// "sinu":      Sinusoidal (Sanson-Flamsteed)
// "somerc":    Swiss. Obl. Mercator
// "stere":     Stereographic
// "sterea":    Oblique Stereographic Alternative
// "gstmerc":   Gauss-Schreiber Transverse Mercator (aka Gauss-Laborde Reunion)
// "tcc":       Transverse Central Cylindrical
// "tcea":      Transverse Cylindrical Equal Area
// "tissot":    Tissot Conic
// "tmerc":     Transverse Mercator
// "tpeqd":     Two Point Equidistant
// "tpers":     Tilted perspective
// "ups":       Universal Polar Stereographic
// "urm5":      Urmaev V
// "urmfps":    Urmaev Flat-Polar Sinusoidal
// "utm":       Universal Transverse Mercator (UTM)
// "vandg":     van der Grinten (I)
// "vandg2":    van der Grinten II
// "vandg3":    van der Grinten III
// "vandg4":    van der Grinten IV
// "vitk1":     Vitkovsky I
// "wag1":      Wagner I (Kavraisky VI)
// "wag2":      Wagner II
// "wag3":      Wagner III
// "wag4":      Wagner IV
// "wag5":      Wagner V
// "wag6":      Wagner VI
// "wag7":      Wagner VII
// "weren":     Werenskiold I
// "wink1":     Winkel I
// "wink2":     Winkel II
// "wintri":    Winkel Tripel

// Supported ellipses:
// "MERIT":     MERIT 1983
// "SGS85":     Soviet Geodetic System 85
// "GRS80":     GRS 1980(IUGG, 1980)
// "IAU76":     IAU 1976
// "airy":      Airy 1830
// "APL4.9":    Appl. Physics. 1965
// "NWL9D":     Naval Weapons Lab., 1965
// "mod_airy":  Modified Airy
// "andrae":    Andrae 1876 (Den., Iclnd.)
// "aust_SA":   Australian Natl & S. Amer. 1969
// "GRS67":     GRS 67(IUGG 1967)
// "bessel":    Bessel 1841
// "bess_nam":  Bessel 1841 (Namibia)
// "clrk66":    Clarke 1866
// "clrk80":    Clarke 1880 mod.
// "CPM":       Comm. des Poids et Mesures 1799
// "delmbr":    Delambre 1810 (Belgium)
// "engelis":   Engelis 1985
// "evrst30":   Everest 1830
// "evrst48":   Everest 1948
// "evrst56":   Everest 1956
// "evrst69":   Everest 1969
// "evrstSS":   Everest (Sabah & Sarawak)
// "fschr60":   Fischer (Mercury Datum) 1960
// "fschr60m":  Modified Fischer 1960
// "fschr68":   Fischer 1968
// "helmert":   Helmert 1906
// "hough":     Hough
// "intl":      International 1909 (Hayford)
// "krass":     Krassovsky, 1942
// "kaula":     Kaula 1961
// "lerch":     Lerch 1979
// "mprts":     Maupertius 1738
// "new_intl":  New International 1967
// "plessis":   Plessis 1817 (France)
// "SEasia":    Southeast Asia
// "walbeck":   Walbeck
// "WGS60":     WGS 60
// "WGS66":     WGS 66
// "WGS72":     WGS 72
// "WGS84":     WGS 84
// "sphere":    Normal Sphere (r=6370997)

// Supported datums:
// Name              Ellipse     Comments
// ----              -------     --------
// "WGS84"           "WGS84"     -
// "GGRS87"          "GRS80"     Greek_Geodetic_Reference_System_1987
// "NAD83"           "GRS80"     North_American_Datum_1983
// "NAD27"           "clrk66"    North_American_Datum_1927
// "potsdam"         "bessel"    Potsdam Rauenberg 1950 DHDN
// "carthage"        "clark80"   Carthage 1934 Tunisia
// "hermannskogel"   "bessel"    Hermannskogel
// "ire65"           "mod_airy"  Ireland 1965
// "nzgd49"          "intl"      New Zealand Geodetic Datum 1949
// "OSGB36"          "airy"      Airy 1830

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
        const std::shared_ptr<class ProjWrap> m_proj;
        std::string m_proj_str;
        bool m_is_valid;
};

#endif
