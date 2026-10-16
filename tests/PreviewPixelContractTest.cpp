#include "Layer/RasterLayer.h"
#include <iostream>

int main()
{
    auto committed=makeRasterLayer(4,4,0x804488CCU);
    committed.pixels[0]=0x00112233U;
    auto pending=makeStrokeCompositeBuffer(4,4);
    const std::vector<RasterSample> samples{{{1,1},0x8066AA44U,0xBBU},{{1,1},0x4066AA44U,0xBBU},{{2,2},0x80112233U,0xFFU},{{0,0},0x00ABCDEFU,0xFFU}};
    accumulateStrokeSamples(pending,samples);
    for(bool erase:{false,true}) {
        auto preview=committed;
        copyStrokePreviewSamplePixels(preview,committed,pending,samples,erase);
        auto reference=committed;
        if(erase) eraseStrokeBufferFromLayer(reference,pending); else compositeStrokeBufferOntoLayer(reference,pending);
        if(preview.pixels!=reference.pixels) return 1;
        copyStrokePreviewSamplePixels(preview,committed,pending,samples,erase);
        if(preview.pixels!=reference.pixels) return 2;
    }
    return 0;
}
