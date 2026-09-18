//
// Created by Justmoong on 2026 May 24.
//

#include "IipeQmlTypes.h"

#include <QtQml/qqml.h>

#include "QtAdapter/BitmapBrushConfig.h"
#include "QtAdapter/BitmapFileApiConfig.h"
#include "QtAdapter/BitmapFileItem.h"
#include "QtAdapter/HighFidelityPointerInput.h"

void registerIipeQmlTypes()
{
    configureHighFidelityPointerInput();

    static bool registered = false;
    if (registered) {
        return;
    }

    qRegisterMetaType<BitmapBrushConfig>("BitmapBrushConfig");
    qRegisterMetaType<BitmapViewportConfig>("BitmapViewportConfig");
    qRegisterMetaType<BitmapRuntimeConfig>("BitmapRuntimeConfig");
    qRegisterMetaType<BitmapFileState>("BitmapFileState");
    qmlRegisterType<BitmapFileItem>("iipe", 1, 0, "BitmapFile");
    registered = true;
}
