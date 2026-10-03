# third_party

Vendored external dependencies. Currently empty by design — the core builds
dependency-free. A possible future addition is Catch2 (tests); see docs/ROADMAP.md. The camera
backend uses V4L2 directly, so no libcamera bindings are needed. Each vendored library gets its own
subdirectory with LICENSE and a VERSION pin.
