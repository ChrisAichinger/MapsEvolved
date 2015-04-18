#ifndef GVG2GTIF_GETOPT
#define	GVG2GTIF_GETOPT

#include <wchar.h>

extern "C" {
int getopt(int argc, wchar_t * const argv[], const wchar_t *optstring);
extern   wchar_t *optarg;
extern   int opterr;
extern   int optind;
extern   int optopt;
}
#endif /* ifndef GVG2GTIF_GETOPT */
