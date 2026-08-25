#include "HighFidelityPointerInput.h"

#include <QCoreApplication>

namespace iipe::detail {

void disableNativePointerEventCoalescing();
bool nativePointerEventCoalescingDisabled();

} // namespace iipe::detail

void configureHighFidelityPointerInput()
{
    QCoreApplication::setAttribute(Qt::AA_CompressHighFrequencyEvents, false);
    QCoreApplication::setAttribute(Qt::AA_CompressTabletEvents, false);
    iipe::detail::disableNativePointerEventCoalescing();
}

bool highFidelityPointerInputConfigured()
{
    return !QCoreApplication::testAttribute(Qt::AA_CompressHighFrequencyEvents)
            && !QCoreApplication::testAttribute(Qt::AA_CompressTabletEvents)
            && iipe::detail::nativePointerEventCoalescingDisabled();
}
