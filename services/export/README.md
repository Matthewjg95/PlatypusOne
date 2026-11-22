# services/export

CAD and image handoff for measured parts.

- `OutlineExport.hpp` — a traced silhouette (`geometry::Outline2`) as
  **Outline Forge JSON** (mesh2cad's importer schema, the same one the Tab5
  ShadowScan applet writes) and as **DXF R12** in millimetres (Fusion 360
  sketch import, KiCad board edges).
- `Png.hpp` — dependency-free PNG encoder for session artifacts.

Planned (ROADMAP): STL/OBJ/PLY writers over `geometry::Mesh`.

Layering: depends only on `platypus::geometry`. Functions return file
contents; the caller decides where they go.
