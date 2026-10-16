#include <iiPaintEngine>
#include <iostream>
int main(int argc,char **argv)
{
    if(argc!=2) { std::cerr<<"Usage: iiPaintEngineExample output.png\n"; return 1; }
    BitmapPaintSession paint;
    if(!paint.createFile(argv[1],256,128,"png",0xFFFFFFFFU)) return 2;
    auto preset=builtInBrushPresets().front(); preset.size=24;
    preset.stroke.noiseEnabled=true; preset.color.enabled=true; preset.color.purity=.2;
    if(!paint.setBrushPreset(preset)) return 3;
    paint.setBrushColor(0xFF3366CCU);
    for(int i=0;i<=100;++i) {
        PointerEvent event; event.device=PointerDeviceKind::Tablet; event.tool=PointerToolKind::Pen;
        event.documentPosition={20+i*2.0,64}; event.pressure=.2+.8*i/100.; event.time=i/100.;
        event.phase=i==0?PointerEventPhase::Press:i==100?PointerEventPhase::Release:PointerEventPhase::Move;
        event.button=PointerButton::Primary; event.primaryButtonDown=i!=100;
        if(!paint.processPointerEvent(event).accepted) return 4;
    }
    return paint.save()?0:5;
}
