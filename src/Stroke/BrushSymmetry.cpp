#include "Stroke/BrushSymmetry.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace {
constexpr double tau = 2 * std::numbers::pi;
constexpr std::size_t maxDabs = 65536;

BrushDab transform(const BrushDab &dab, const BrushSymmetrySettings &s, double rotation, bool reflect)
{
    BrushDab copy = dab;
    double x = dab.position.x - s.center.x, y = dab.position.y - s.center.y;
    if (reflect) {
        const double c = std::cos(2*s.angleRadians), sn = std::sin(2*s.angleRadians);
        const double nextX = c*x + sn*y;
        y = sn*x - c*y; x = nextX;
        copy.rotationRadians = 2*s.angleRadians + std::numbers::pi - dab.rotationRadians;
        copy.textureDirectionRadians = 2*s.angleRadians - dab.textureDirectionRadians;
        copy.flipX = !copy.flipX;
        copy.symmetryReflected = !copy.symmetryReflected;
        copy.symmetryRotationRadians = 2*s.angleRadians + std::numbers::pi - dab.symmetryRotationRadians;
    }
    const double c = std::cos(rotation), sn = std::sin(rotation);
    copy.position = {s.center.x + c*x - sn*y, s.center.y + sn*x + c*y};
    copy.rotationRadians = std::remainder(copy.rotationRadians + rotation, tau);
    copy.textureDirectionRadians = std::remainder(copy.textureDirectionRadians + rotation, tau);
    copy.symmetryRotationRadians = std::remainder(copy.symmetryRotationRadians + rotation, tau);
    if (!std::isfinite(copy.position.x) || !std::isfinite(copy.position.y)
            || std::abs(copy.position.x)>1e9 || std::abs(copy.position.y)>1e9)
        throw std::invalid_argument("symmetry produces coordinates outside the document range");
    return copy;
}
}

std::vector<BrushDab> applyBrushSymmetry(BrushDabSpan dabs, const BrushSymmetrySettings &settings)
{
    const auto mode = static_cast<int>(settings.mode);
    if (mode<0 || mode>static_cast<int>(BrushSymmetryMode::Mandala)
            || !std::isfinite(settings.center.x) || !std::isfinite(settings.center.y)
            || std::abs(settings.center.x)>1e9 || std::abs(settings.center.y)>1e9
            || !std::isfinite(settings.angleRadians) || std::abs(settings.angleRadians)>1024
            || settings.segments<2 || settings.segments>64)
        throw std::invalid_argument("invalid brush symmetry settings");
    if (dabs.size()>maxDabs || (dabs.size()!=0 && dabs.data()==nullptr))
        throw std::length_error("symmetry input exceeds the stamp budget");
    if (dabs.empty()) return {};
    std::vector<BrushDab> out;
    const std::size_t copies = settings.mode==BrushSymmetryMode::Mandala ? 2*settings.segments
            : settings.mode==BrushSymmetryMode::Radial ? settings.segments
            : settings.mode==BrushSymmetryMode::DualAxis ? 4
            : settings.mode==BrushSymmetryMode::None ? 1 : 2;
    out.reserve(std::min(maxDabs,dabs.size()*copies));
    for (const auto &dab : dabs) {
        const auto begin = out.size();
        auto emit = [&](const BrushDab &copy) {
            if (!std::isfinite(copy.position.x) || !std::isfinite(copy.position.y)
                    || std::abs(copy.position.x)>1e9 || std::abs(copy.position.y)>1e9
                    || !std::isfinite(copy.rotationRadians) || !std::isfinite(copy.textureDirectionRadians)
                    || !std::isfinite(copy.symmetryRotationRadians))
                throw std::invalid_argument("symmetry requires finite bitmap stamp geometry");
            for (auto i=begin;i<out.size();++i)
                if (std::hypot(out[i].position.x-copy.position.x,out[i].position.y-copy.position.y)<1e-8)
                    return;
            if (out.size()==maxDabs) throw std::length_error("symmetry exceeds 65536 stamps per event");
            out.push_back(copy);
        };
        emit(dab);
        auto s=settings;
        switch (s.mode) {
            case BrushSymmetryMode::None: break;
            case BrushSymmetryMode::Vertical:
                s.angleRadians+=std::numbers::pi/2; emit(transform(dab,s,0,true)); break;
            case BrushSymmetryMode::Horizontal: emit(transform(dab,s,0,true)); break;
            case BrushSymmetryMode::DualAxis:
                emit(transform(dab,s,0,true)); s.angleRadians+=std::numbers::pi/2;
                emit(transform(dab,s,0,true)); emit(transform(dab,s,std::numbers::pi,false)); break;
            case BrushSymmetryMode::Radial:
            case BrushSymmetryMode::Mandala:
                for (std::uint32_t i=0;i<s.segments;++i) {
                    const auto rotation=tau*i/s.segments;
                    if (i) emit(transform(dab,s,rotation,false));
                    if (s.mode==BrushSymmetryMode::Mandala) emit(transform(dab,s,rotation,true));
                }
                break;
        }
    }
    return out;
}
