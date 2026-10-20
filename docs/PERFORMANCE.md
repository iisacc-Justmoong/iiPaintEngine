# Filter pipeline costs

The ARGB box blur maintains exact 64-bit column sums and slides a horizontal
window across them. Its work is O(width * height), independent of blur radius,
instead of repeating a square neighborhood scan for each selected pixel. It
keeps the existing source snapshot and adds O(width) scratch memory (32 bytes per
column). A sparse selection still requires scanning the image.

Edges use only in-bounds samples. Division happens once after summing integer
channels, without intermediate rounding or a separable approximation. Selection
predicates and rounding of finite radii are preserved. Oversized windows are
clipped before integer conversion; radii below one use a one-pixel radius.
Empty surfaces are no-ops.

`FilterWindowParityTest` checks every pixel against an independent scalar
reference across empty/single-row/single-column images, random ARGB data, radius
rounding, oversized windows, rectangles, inverted selections and alpha masks.
Existing editing/pipeline tests cover disabled nodes and chained filters.
