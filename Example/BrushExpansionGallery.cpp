#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include "iiPaintEngine"

int main(int argc,char **argv)
{
    const auto path=argc>1 ? std::filesystem::path(argv[1]) : std::filesystem::path("build/brush-expansion.png");
    BitmapPaintSession session;
    if(!session.createFile(path,960,320,"png",0xFF101B29U)) return 1;
    for(int panel=0;panel<3;++panel) {
        BrushPreset preset;
        preset.size=panel==1 ? 6 : 4;
        preset.flow=.55F;preset.hardness=.7F;preset.stroke.spacingRatio=.18;
        preset.stroke.symmetry.center={160.0+320*panel,160};
        preset.stroke.symmetry.mode=panel==0 ? BrushSymmetryMode::Radial
                : panel==1 ? BrushSymmetryMode::Mandala : BrushSymmetryMode::DualAxis;
        preset.stroke.symmetry.segments=panel==0 ? 16 : 12;
        preset.dynamics.sizeVariation.enabled=true;
        preset.dynamics.sizeVariation.control=BrushDynamicsControl::PenPressure;
        preset.dynamics.sizeVariation.minimum=.2;
        if(panel==1) {
            preset.tip={3,2,{std::byte{255},std::byte{220},std::byte{40},
                            std::byte{190},std::byte{120},std::byte{0}}};
            preset.shape.angleMode=BrushAngleMode::StrokeDirection;
        }
        if(panel==2) {
            preset.stroke.smoothingEnabled=true;preset.stroke.smoothing=1;
            preset.stroke.smoothingMode=BrushSmoothingMode::PulledString;
            preset.stroke.smoothingRadius=5;preset.stroke.smoothingCatchUpOnEnd=true;
        }
        if(!session.setBrushPreset(preset)) return 2;
        session.setBrushColor(panel==0 ? 0xFF50DCC9U : panel==1 ? 0xFFFFB75CU : 0xFFCA9AFFU);
        for(int pass=0;pass<3;++pass) for(int i=0;i<=160;++i) {
            const double t=i/160.0;
            const double radius=18+pass*18+65*t;
            const double angle=.11+pass*.04+.22*std::sin(t*std::numbers::pi);
            PointerEvent event;
            event.device=PointerDeviceKind::Tablet;event.tool=PointerToolKind::Pen;
            event.phase=i==0 ? PointerEventPhase::Press : i==160 ? PointerEventPhase::Release : PointerEventPhase::Move;
            event.button=PointerButton::Primary;event.primaryButtonDown=i!=160;
            event.documentPosition={preset.stroke.symmetry.center.x+radius*std::cos(angle),
                                    160+radius*std::sin(angle)};
            event.pressure=.3+.7*std::sin(t*std::numbers::pi);
            event.time=panel*20+pass*3+t;
            if(!session.processPointerEvent(event).accepted) {
                std::cerr<<session.lastError()<<'\n';return 3;
            }
        }
    }
    if(!session.save()) {std::cerr<<session.lastError()<<'\n';return 4;}
    std::cout<<path<<'\n';
}
