#!/usr/bin/python3

'''Prepare SVG logo for export to different icon sizes.

The MapsEvolved icon is needed in different sizes (16x16 to 64x64).
Each icon should have a one-pixel black plus a one-pixel white border. To
achieve this, this script generates SVG (and optionally PNG) files with
correctly sized borders from a master SVG.
'''

import sys
import os
import io
import argparse
import subprocess
from xml.etree import ElementTree as ET

# Output icon sizes. Tuple for immutability.
ICON_SIZES = (16, 32, 48, 64)

def set_stroke_width(node, width):
    """Set stroke width in pixels on node"""
    style = node.attrib['style'].split(';')
    style = dict(entry.split(':', 1) for entry in style)
    style['stroke-width'] = str(width)
    node.attrib['style'] = ';'.join(k + ':' + style[k] for k in style)

def scale_svg(fname, svg, export_px, svg_size_px):
    """Scale SVG to desired export size

    Scale an SVG, identified by ``fname`` and the XML tree ``svg``, for output
    of icons with size ``export_px``. ``svg_size_px`` is the size of the SVG
    page in pixels.

    The output is written to a file, the filename is derived from the ``fname``
    and ``export_sz``. The output filename is returned.
    """

    one_export_px_in_px = svg_size_px / export_px

    rect_white = svg.find('*[@id="rectWhite"]')
    rect_black = svg.find('*[@id="rectBlack"]')
    set_stroke_width(rect_white, 4 * one_export_px_in_px)
    set_stroke_width(rect_black, 2 * one_export_px_in_px)

    svgoutput = '{}-{}.svg'.format(os.path.splitext(fname)[0], export_px)
    header = '<?xml version="1.0" encoding="UTF-8" standalone="no"?>\n'
    xml = ET.tostring(svg.getroot(), encoding='unicode')
    with io.open(svgoutput, 'w', encoding='UTF-8') as f:
        f.write(header + xml)

    return svgoutput


def scale_svg_multiple(fname, icon_sizes, *, inkscape=None):
    """Scale an SVG to multiple output sizes

    Scale the SVG ``fname`` for output to several destination icon sizes.
    ``icon_sizes`` is an iterable specifying the desired output sizes in
    pixels.

    If ``inkscape`` is not None, it must be the path to an inkscape executable.
    If this argument is supplied, PNG files are generated as well.
    """

    svg = ET.parse(fname)
    attr = svg.getroot().attrib

    viewbox = [float(s) for s in attr['viewBox'].split()]
    minx_px, miny_px, width_px, height_px = viewbox

    assert minx_px == 0
    assert miny_px == 0
    assert width_px == height_px

    for export_px in icon_sizes:
        svgoutput = scale_svg(fname, svg, export_px, width_px)

        if not inkscape:
            continue

        pngoutput = '{}-{}.png'.format(os.path.splitext(fname)[0], export_px)
        subprocess.check_call([inkscape, svgoutput, '--export-png', pngoutput,
                               '-w', str(export_px), '-h', str(export_px)])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', metavar='input.svg',
                        help='master SVG input file name')
    parser.add_argument('--inkscape',
                        help='path to inkscape, produce PNGs if present')
    args = parser.parse_args()

    scale_svg_multiple(args.input, ICON_SIZES, inkscape=args.inkscape)

if __name__ == '__main__':
    main()
