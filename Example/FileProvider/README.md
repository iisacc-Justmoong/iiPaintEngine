# Bitmap files through iiFileProvider

This separate consumer composes iiPaintEngine's `BitmapFileIO` contract with
independently installed iiFileProvider Native. The provider atomically publishes
complete bytes and schedules file details and preview generation. The three
existing bitmap examples are built here; the engine itself has no peer SDK link.
Install this application-side adapter separately and use
`find_package(iiPaintEngineFileProvider CONFIG REQUIRED)` with
`iiPaintEngine::FileProvider` when reusing it.
The tests clear inherited dynamic-library search overrides so the selected engine
package is exercised instead of an older globally installed library. Applications
must rebuild against a matching engine binary and headers.
See [the bitmap I/O contract](../../docs/bitmap-io.md).
