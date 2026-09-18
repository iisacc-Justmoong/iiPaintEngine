#import <AppKit/NSEvent.h>

namespace iipe::detail {

void disableNativePointerEventCoalescing()
{
    [NSEvent setMouseCoalescingEnabled:NO];
}

bool nativePointerEventCoalescingDisabled()
{
    return ![NSEvent isMouseCoalescingEnabled];
}

} // namespace iipe::detail
