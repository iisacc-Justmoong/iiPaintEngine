# File creation ownership in iiPaintEngine

Bitmap file encoders publish through native provider streams. Raster encoding and painting remain in the engine; the storage dependency imports no Qt or GUI adapter.

The common [ownership contract](../../iiFileProvider/docs/FILE_OWNERSHIP.md) defines stream, staging, control-file and
installed-package rules. Source regression tests remain with this module; the
workspace creation audit also checks these boundaries. Build/deployment and target
runtime validation are separate evidence.

Changed boundaries:


- `src/BitmapFile/BitmapCodec.cpp`
