//
// Created by Justmoong on 2026 May 24.
//

#include "IipeQmlTypes.h"

#include <QtQml/qqml.h>

#include "QtAdapter/CanvasAdapter.h"
#include "QtAdapter/CanvasApiConfig.h"
#include "QtAdapter/CanvasBrushConfig.h"
#include "QtAdapter/PaintCanvasItem.h"

void registerIipeQmlTypes()
{
    static bool registered = false;
    if (registered) {
        return;
    }

    qRegisterMetaType<CanvasBrushConfig>("CanvasBrushConfig");
    qRegisterMetaType<CanvasViewportConfig>("CanvasViewportConfig");
    qRegisterMetaType<CanvasRuntimeConfig>("CanvasRuntimeConfig");
    qRegisterMetaType<CanvasStateSnapshot>("CanvasStateSnapshot");
    qmlRegisterType<PaintCanvasItem>("iipe", 1, 0, "Canvas");
    qmlRegisterType<CanvasAdapter>("iipe", 1, 0, "CanvasAdapter");
    registered = true;
}
