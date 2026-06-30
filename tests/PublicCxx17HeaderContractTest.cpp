#include <iiPaintEngine>

#include <vector>

int main()
{
    std::vector<BrushDab> dabs;
    BrushDabSpan emptyDabs;
    BrushDabSpan dabView{dabs};

    Rasterizer rasterizer;
    RasterProjection projection;
    const std::vector<RasterSample> samples = projectBrushDabs(dabView, rasterizer, projection);
    const std::vector<DevicePixelRect> bounds = deviceBoundsForBrushDabs(dabView, rasterizer, projection);

    return emptyDabs.empty()
            && dabView.data() == dabs.data()
            && dabView.size() == dabs.size()
            && samples.empty()
            && bounds.empty()
            ? 0
            : 1;
}
