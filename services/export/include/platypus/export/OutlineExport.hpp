// PlatypusOS services — CAD handoff for measured outlines.
//
// A session's finished outline leaves PlatypusOne in two formats:
//
//   Outline Forge JSON  mesh2cad's importer schema, identical to what the
//                       Tab5 ShadowScan applet writes, so both instruments
//                       feed one pipeline:
//                         { "format": "shadowscan-outline", "units": "px",
//                           "scale_mm_per_unit": <mm/px>,
//                           "outlines": [ [[x,y], ...], ... ] }
//                       Outline Forge scales every point and nests contained
//                       loops as holes, so bores travel as extra loops.
//
//   DXF (R12, ASCII)    millimetres, CAD axes (y up), origin at the outline's
//                       bounding-box minimum; one closed POLYLINE per loop on
//                       layer OUTLINE (outer) or HOLES. Imports into Fusion
//                       360 as a sketch and into KiCad as board edges.
//
// Pure functions returning file contents; writing is the caller's concern.
// No dependencies beyond the shared geometry types (layering rule).
#pragma once

#include <platypus/geometry/Types.hpp>

#include <string>

namespace platypus::exporter {

/// Outline Forge (mesh2cad) JSON. Points stay in pixels, as ShadowScan
/// writes them; the scale converts. Floats, not rounded — rounding would
/// stair-step the outline by up to a pixel of real scale.
[[nodiscard]] std::string outlineForgeJson(const geometry::Outline2& outlinePx, double mmPerPx);

/// DXF R12 in millimetres, y flipped from image axes to CAD axes.
[[nodiscard]] std::string outlineDxf(const geometry::Outline2& outlinePx, double mmPerPx);

}  // namespace platypus::exporter
