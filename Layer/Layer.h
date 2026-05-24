//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <QObject>

class Layer : public QObject {
    Q_OBJECT

public:
    explicit Layer(QObject *parent = nullptr);

};
