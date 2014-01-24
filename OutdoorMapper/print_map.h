#ifndef ODM__PRINT_MAP_H
#define ODM__PRINT_MAP_H

#include "winwrap.h"

class MapPrinter : public IPrintClient {
    public:
        MapPrinter(const std::shared_ptr<class MapDisplayManager> &display,
                   double scale_factor);
        bool Print(HDC hdc);

    private:
        std::shared_ptr<class MapDisplayManager> m_display;
        double m_scale;
};

#endif
