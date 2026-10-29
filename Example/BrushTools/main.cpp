#include <iiBrushTools/Gesture.h>
#include <iostream>
int main()
{
    ii::brush::Controls controls;
    StrokePoint samples[2];samples[0].position={16,32};samples[1].position={144,32};samples[1].time=1;
    const auto result=ii::brush::renderGesture(samples,ii::brush::Tool::Paint,controls,{{0,0,192,96}});
    if(!result){std::cerr<<result.error().message<<'\n';return 1;}
    std::cout<<"length="<<result->length<<" dabs="<<result->dabCount<<" tiles="<<result->tiles.size()<<'\n';
}
