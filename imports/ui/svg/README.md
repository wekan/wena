# SVG source assets

All first-party UI artwork and native theme asset definitions originate here as
SVG. The original artwork, converter and C89 renderer are MIT licensed under the
repository's [license](../../../LICENSE). The runtime uses the pinned Nuklear
MIT alternative; there is no additional SVG library or GPL dependency.

`board.svg` compiles to four native vector primitives. `native-light.svg` defines
semantic color swatches and logical spacing measurements. Color names and RGB
values are checked against `models/color.c`, keeping the shared WeKan palette
canonical. Theme selection and complete web/native visual parity remain roadmap
work; this is the existing native light default expressed as SVG source.

Run `python3 scripts/compile_svg.py` from the repository root after editing SVG,
then `./build.sh tests svg`. The desktop build checks that generated headers are
current. It links geometry and semantic tokens, not XML, PNGs or multiple artwork
sizes. Nuklear converts the primitives to the renderer's commands at the requested
size; `currentColor` uses the current native text color. Aspect ratio is retained.
The board icon scales with the actual title font, while titles stay live text.
Existing interactive widgets keep their event handling and layout.

The intentionally bounded SVG subset is zero-origin `viewBox`, solid `#RRGGBB`
and `currentColor`/`none` paints, rectangles (optional circular corner radius),
circles and lines. Shapes must fit the viewBox, including strokes. The compiler
rejects unsupported elements, CSS, scripts, external resources, images, filters,
transforms, gradients, declarations and excessive input. It never silently drops
unsupported artwork. Add a reviewed primitive and its converter/renderer tests
when an asset needs it. This is not a general-purpose SVG viewer or attachment
loader. New source assets must be registered in the conversion catalog.

The native drawing API validates a complete bounded command list before drawing,
uses no raster texture cache, and fits its output inside Nuklear's coordinate
limits. Fractional strokes and corners ultimately follow the existing renderer's
pixel rounding, so this does not promise identical antialiasing at every size.
HTML4 clients retain their existing ASCII controls; SVG is not a new browser
requirement. Fonts retain their separate, licensed outline-font pipeline.

Historical screenshots under `docs/images` are documentation, not UI assets.
They were converted with the original MIT `scripts/png_to_svg.py`, using only the
Python standard library. Paths preserve each original RGBA pixel, including
rasterized text, rather than reconstructing or changing historical screenshots.
Their SVGs are larger than the compressed originals and are never embedded into
the executable. `docs/images/provenance.json` records source and decoded-pixel
hashes. Tests independently reconstruct the paths and verify those pixel hashes.
