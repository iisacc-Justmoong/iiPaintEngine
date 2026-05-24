//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <QObject>

class DrawingSurface : public QObject {
    Q_OBJECT

public:
    explicit DrawingSurface(QObject *parent = nullptr);

};
