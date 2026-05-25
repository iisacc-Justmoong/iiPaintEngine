//
// Created by Justmoong on 2026 May 24.
//

#include "IipeQmlTypes.h"

#include <QtQml/qqml.h>

#include "QtAdapter/PaintCanvasItem.h"

void registerIipeQmlTypes()
{
    static bool registered = false;
    if (registered) {
        return;
    }

    qmlRegisterType<PaintCanvasItem>("iipe", 1, 0, "Canvas");
    registered = true;
}
