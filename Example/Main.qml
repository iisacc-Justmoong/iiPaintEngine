pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as QC
import QtQuick.Layouts
import LVRS 1.0 as LV
import iipe 1.0 as Iipe

LV.ApplicationWindow {
    id: root
    objectName: "iiPaintEngineExampleWindow"

    visible: true
    width: 1280
    height: 820
    desktopMinWidth: 980
    desktopMinHeight: 640
    usePlatformSafeMargin: true
    navigationEnabled: false
    autoAttachRuntimeEvents: true
    title: "iiPaintEngine"
    subtitle: "Painting Demo"
    windowColor: "#111318"

    readonly property rect workspaceRect: layoutSafeAreaBounds.width > 0 ? layoutSafeAreaBounds : Qt.rect(0, 0, width, height)
    readonly property bool compactDemoLayout: workspaceRect.width < 980
    readonly property int edgeGap: compactDemoLayout ? 10 : 14
    readonly property int toolbarHeight: 64
    readonly property int panelWidth: compactDemoLayout ? 244 : 286
    readonly property int panelTop: Math.round(workspaceRect.y + edgeGap + toolbarHeight + edgeGap)
    readonly property int panelHeight: Math.max(1, Math.round(workspaceRect.height - toolbarHeight - edgeGap * 3))
    readonly property int canvasLeft: compactDemoLayout ? Math.round(workspaceRect.x + edgeGap) : Math.round(workspaceRect.x + edgeGap + panelWidth + edgeGap)
    readonly property int canvasTop: panelTop
    readonly property int canvasWidth: compactDemoLayout ? Math.max(1, Math.round(workspaceRect.width - edgeGap * 2)) : Math.max(1, Math.round(workspaceRect.width - panelWidth - edgeGap * 3))
    readonly property int canvasHeight: compactDemoLayout ? Math.max(1, Math.round(panelHeight - panelWidth - edgeGap)) : panelHeight
    readonly property var swatches: [
        {
            "name": "Ink",
            "color": "#101318"
        },
        {
            "name": "Lake",
            "color": "#2563eb"
        },
        {
            "name": "Carmine",
            "color": "#d7264c"
        },
        {
            "name": "Moss",
            "color": "#2f7d52"
        },
        {
            "name": "Amber",
            "color": "#f5a524"
        },
        {
            "name": "Violet",
            "color": "#7c3aed"
        }
    ]
    readonly property bool demoReady: demoCanvas.width > 0 && demoCanvas.height > 0 && paintControls.width > 0 && clearButton !== null && livePreviewToggle !== null && pressureCurveGraph !== null && pressureOpacityArgumentToggle !== null

    property int activeSwatchIndex: 0
    property color activeBrushColor: swatches[activeSwatchIndex].color
    property real currentBrushSize: 18.0
    property real currentFlow: 1.0
    property real currentOpacity: 1.0
    property real currentHardness: 1.0
    property real currentSpacingRatio: 0.0
    property real currentPressureCurveMinimum: 0.0
    property real currentPressureCurveCenter: 0.5
    property real currentPressureCurveMaximum: 1.0
    property real currentZoom: 1.0
    property bool flowArgumentEnabled: true
    property bool opacityArgumentEnabled: true
    property bool pressureOpacityArgumentEnabled: true
    property bool hardnessArgumentEnabled: true
    property bool spacingArgumentEnabled: true

    function applyBrushSettings() {
        demoCanvas.setBrush(currentBrushSize, activeBrushColor, currentFlow, currentOpacity);
        demoCanvas.brushFlowEnabled = flowArgumentEnabled;
        demoCanvas.brushOpacityEnabled = opacityArgumentEnabled;
        demoCanvas.pressureToOpacityEnabled = pressureOpacityArgumentEnabled;
        demoCanvas.brushHardness = currentHardness;
        demoCanvas.brushHardnessEnabled = hardnessArgumentEnabled;
        demoCanvas.brushSpacingRatio = currentSpacingRatio;
        demoCanvas.brushSpacingEnabled = spacingArgumentEnabled;
        demoCanvas.pressureCurveMinimum = currentPressureCurveMinimum;
        demoCanvas.pressureCurveCenter = currentPressureCurveCenter;
        demoCanvas.pressureCurveMaximum = currentPressureCurveMaximum;
        demoCanvas.livePreviewEnabled = livePreviewToggle.checked;
    }

    function clamp01(value) {
        return Math.max(0, Math.min(1, value));
    }

    function setPressureCurve(minimum, center, maximum) {
        var nextMinimum = clamp01(minimum);
        var nextMaximum = Math.max(clamp01(maximum), nextMinimum);
        var nextCenter = Math.max(nextMinimum, Math.min(clamp01(center), nextMaximum));
        currentPressureCurveMinimum = nextMinimum;
        currentPressureCurveCenter = nextCenter;
        currentPressureCurveMaximum = nextMaximum;
        applyBrushSettings();
    }

    function chooseSwatch(index) {
        if (index < 0 || index >= swatches.length)
            return;
        activeSwatchIndex = index;
        activeBrushColor = swatches[index].color;
        applyBrushSettings();
    }

    function setZoom(value) {
        currentZoom = Math.max(0.25, Math.min(3.0, value));
        demoCanvas.zoom = currentZoom;
    }

    Component.onCompleted: {
        demoCanvas.resetView();
        applyBrushSettings();
    }

    Rectangle {
        id: topBar
        objectName: "paintTopBar"
        x: Math.round(root.workspaceRect.x + root.edgeGap)
        y: Math.round(root.workspaceRect.y + root.edgeGap)
        width: Math.max(1, Math.round(root.workspaceRect.width - root.edgeGap * 2))
        height: root.toolbarHeight
        radius: 8
        color: "#1b2027"
        border.color: "#2b3440"
        border.width: 1

        LV.HStack {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 12
            spacing: 12

            LV.Label {
                text: "iiPaintEngine"
                style: header
                color: "#f2f5f8"
                Layout.preferredWidth: 180
                Layout.alignment: Qt.AlignVCenter
            }

            LV.Label {
                text: demoCanvas.strokeCount + " strokes"
                style: body
                color: "#aab6c2"
                Layout.preferredWidth: 110
                Layout.alignment: Qt.AlignVCenter
            }

            LV.Label {
                objectName: "inputPressureLabel"
                text: demoCanvas.inputDevice + " " + Math.round(demoCanvas.inputPressure * 100) + "%"
                style: body
                color: demoCanvas.inputDevice === "tablet" ? "#f5a524" : "#8fa0b2"
                Layout.preferredWidth: 120
                Layout.alignment: Qt.AlignVCenter
            }

            Rectangle {
                width: 1
                height: 28
                color: "#33404d"
                Layout.alignment: Qt.AlignVCenter
            }

            LV.LabelButton {
                id: clearButton
                objectName: "clearButton"
                text: "Clear"
                tone: LV.AbstractButton.Destructive
                Layout.alignment: Qt.AlignVCenter
                onClicked: demoCanvas.clear()
            }

            LV.LabelButton {
                objectName: "resetViewButton"
                text: "Reset View"
                tone: LV.AbstractButton.Default
                Layout.alignment: Qt.AlignVCenter
                onClicked: {
                    root.setZoom(1.0);
                    demoCanvas.resetView();
                }
            }

            LV.LabelButton {
                objectName: "zoomOutButton"
                text: "50%"
                tone: LV.AbstractButton.Borderless
                Layout.alignment: Qt.AlignVCenter
                onClicked: root.setZoom(0.5)
            }

            LV.LabelButton {
                objectName: "zoomActualButton"
                text: "100%"
                tone: LV.AbstractButton.Borderless
                Layout.alignment: Qt.AlignVCenter
                onClicked: root.setZoom(1.0)
            }

            LV.LabelButton {
                objectName: "zoomInButton"
                text: "200%"
                tone: LV.AbstractButton.Borderless
                Layout.alignment: Qt.AlignVCenter
                onClicked: root.setZoom(2.0)
            }

            Item {
                Layout.fillWidth: true
                Layout.minimumWidth: 16
            }

            LV.Label {
                text: "Live"
                style: body
                color: "#c7d2df"
                Layout.alignment: Qt.AlignVCenter
            }

            LV.ToggleSwitch {
                id: livePreviewToggle
                objectName: "livePreviewToggle"
                checked: true
                Layout.alignment: Qt.AlignVCenter
                onCheckedChanged: root.applyBrushSettings()
            }
        }
    }

    Rectangle {
        id: paintControls
        objectName: "paintControls"
        x: Math.round(root.workspaceRect.x + root.edgeGap)
        y: root.compactDemoLayout ? Math.round(root.canvasTop + root.canvasHeight + root.edgeGap) : root.panelTop
        width: root.compactDemoLayout ? Math.max(1, Math.round(root.workspaceRect.width - root.edgeGap * 2)) : root.panelWidth
        height: root.compactDemoLayout ? root.panelWidth : root.panelHeight
        visible: height > 140
        radius: 8
        color: "#171c23"
        border.color: "#2b3440"
        border.width: 1

        Column {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 14

            LV.Label {
                width: parent.width
                text: "Brush"
                style: header2
                color: "#f2f5f8"
            }

            Rectangle {
                width: parent.width
                height: 86
                radius: 8
                color: "#202832"

                Rectangle {
                    width: Math.max(12, Math.min(64, root.currentBrushSize))
                    height: width
                    radius: width / 2
                    anchors.centerIn: parent
                    color: root.activeBrushColor
                    opacity: root.currentOpacity
                    border.color: "#f7fafc"
                    border.width: 1
                }
            }

            Grid {
                width: parent.width
                columns: 6
                spacing: 8

                Repeater {
                    model: root.swatches.length

                    Rectangle {
                        required property int index

                        width: Math.floor((paintControls.width - 32 - 40) / 6)
                        height: width
                        radius: 6
                        color: root.swatches[index].color
                        border.color: index === root.activeSwatchIndex ? "#f8fafc" : "#465465"
                        border.width: index === root.activeSwatchIndex ? 2 : 1

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: root.chooseSwatch(index)
                        }
                    }
                }
            }

            BrushSlider {
                objectName: "sizeSlider"
                width: parent.width
                label: "Size"
                valueText: Math.round(root.currentBrushSize) + " px"
                2
                to: 72
                value: root.currentBrushSize
                onMoved: function (value) {
                    root.currentBrushSize = value;
                    root.applyBrushSettings();
                }
            }

            BrushSlider {
                objectName: "flowSlider"
                width: parent.width
                label: "Flow"
                valueText: Math.round(root.currentFlow * 100) + "%"
                0.05
                to: 1
                value: root.currentFlow
                toggleVisible: true
                toggleObjectName: "flowArgumentToggle"
                toggleChecked: root.flowArgumentEnabled
                onMoved: function (value) {
                    root.currentFlow = value;
                    root.applyBrushSettings();
                }
                onToggleChanged: function (checked) {
                    root.flowArgumentEnabled = checked;
                    root.applyBrushSettings();
                }
            }

            BrushSlider {
                objectName: "opacitySlider"
                width: parent.width
                label: "Opacity"
                valueText: Math.round(root.currentOpacity * 100) + "%"
                0.05
                to: 1
                value: root.currentOpacity
                toggleVisible: true
                toggleObjectName: "opacityArgumentToggle"
                toggleChecked: root.opacityArgumentEnabled
                onMoved: function (value) {
                    root.currentOpacity = value;
                    root.applyBrushSettings();
                }
                onToggleChanged: function (checked) {
                    root.opacityArgumentEnabled = checked;
                    root.applyBrushSettings();
                }
            }

            Item {
                objectName: "pressureOpacityControl"
                width: parent.width
                height: 28

                LV.Label {
                    x: 0
                    y: 4
                    width: Math.max(1, parent.width - pressureOpacityArgumentToggle.width - 12)
                    height: 20
                    text: "Pressure Opacity"
                    style: caption
                    color: "#c7d2df"
                    verticalAlignment: Text.AlignVCenter
                }

                LV.ToggleSwitch {
                    id: pressureOpacityArgumentToggle
                    objectName: "pressureOpacityArgumentToggle"
                    x: Math.round(parent.width - width)
                    y: 3
                    width: 42
                    height: 22
                    checked: root.pressureOpacityArgumentEnabled
                    onCheckedChanged: {
                        root.pressureOpacityArgumentEnabled = checked;
                        root.applyBrushSettings();
                    }
                }
            }

            BrushSlider {
                objectName: "hardnessSlider"
                width: parent.width
                label: "Hardness"
                valueText: Math.round(root.currentHardness * 100) + "%"
                0.05
                to: 1
                value: root.currentHardness
                toggleVisible: true
                toggleObjectName: "hardnessArgumentToggle"
                toggleChecked: root.hardnessArgumentEnabled
                onMoved: function (value) {
                    root.currentHardness = value;
                    root.applyBrushSettings();
                }
                onToggleChanged: function (checked) {
                    root.hardnessArgumentEnabled = checked;
                    root.applyBrushSettings();
                }
            }

            BrushSlider {
                objectName: "spacingSlider"
                width: parent.width
                label: "Spacing"
                valueText: Math.round(root.currentSpacingRatio * 100) + "%"
                0
                to: 1
                value: root.currentSpacingRatio
                toggleVisible: true
                toggleObjectName: "spacingArgumentToggle"
                toggleChecked: root.spacingArgumentEnabled
                onMoved: function (value) {
                    root.currentSpacingRatio = value;
                    root.applyBrushSettings();
                }
                onToggleChanged: function (checked) {
                    root.spacingArgumentEnabled = checked;
                    root.applyBrushSettings();
                }
            }

            PressureCurveEditor {
                id: pressureCurveGraph
                objectName: "pressureCurveGraph"
                width: parent.width
                minimum: root.currentPressureCurveMinimum
                center: root.currentPressureCurveCenter
                maximum: root.currentPressureCurveMaximum
            }

        }
    }

    Rectangle {
        id: canvasFrame
        objectName: "canvasFrame"
        x: root.canvasLeft
        y: root.canvasTop
        width: root.canvasWidth
        height: root.canvasHeight
        radius: 8
        color: "#f5f0e7"
        border.color: "#465465"
        border.width: 1
        clip: true

        Repeater {
            model: Math.max(0, Math.ceil(canvasFrame.width / 32))
            Rectangle {
                required property int index

                x: index * 32
                y: 0
                width: 1
                height: canvasFrame.height
                color: "#e0d8ca"
                opacity: 0.45
            }
        }

        Repeater {
            model: Math.max(0, Math.ceil(canvasFrame.height / 32))
            Rectangle {
                required property int index

                x: 0
                y: index * 32
                width: canvasFrame.width
                height: 1
                color: "#e0d8ca"
                opacity: 0.45
            }
        }

        Iipe.Canvas {
            id: demoCanvas
            objectName: "demoCanvas"
            anchors.fill: parent
            anchors.margins: 18
            documentX: 0
            documentY: 0
            zoom: root.currentZoom
            canvasDevicePixelRatio: Screen.devicePixelRatio > 0 ? Screen.devicePixelRatio : 1
            brushColor: root.activeBrushColor
            brushSize: root.currentBrushSize
            brushFlow: root.currentFlow
            brushFlowEnabled: root.flowArgumentEnabled
            brushOpacity: root.currentOpacity
            brushOpacityEnabled: root.opacityArgumentEnabled
            pressureToOpacityEnabled: root.pressureOpacityArgumentEnabled
            brushHardness: root.currentHardness
            brushHardnessEnabled: root.hardnessArgumentEnabled
            brushSpacingRatio: root.currentSpacingRatio
            brushSpacingEnabled: root.spacingArgumentEnabled
            pressureCurveMinimum: root.currentPressureCurveMinimum
            pressureCurveCenter: root.currentPressureCurveCenter
            pressureCurveMaximum: root.currentPressureCurveMaximum
            livePreviewEnabled: livePreviewToggle.checked
        }
    }

    component PressureCurveEditor: Item {
        id: curveRoot

        property real minimum: 0.0
        property real center: 0.5
        property real maximum: 1.0

        height: 138

        onMinimumChanged: graphCanvas.requestPaint()
        onCenterChanged: graphCanvas.requestPaint()
        onMaximumChanged: graphCanvas.requestPaint()

        LV.Label {
            x: 0
            y: 0
            width: curveRoot.width
            height: 18
            text: "Pressure Curve"
            style: caption
            color: "#c7d2df"
        }

        Canvas {
            id: graphCanvas
            x: 0
            y: 20
            width: curveRoot.width
            height: 72

            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                ctx.clearRect(0, 0, width, height);
                ctx.strokeStyle = "#33404d";
                ctx.lineWidth = 1;
                ctx.strokeRect(0.5, 0.5, width - 1, height - 1);

                var minY = height * (1.0 - curveRoot.minimum);
                var centerX = width * 0.5;
                var centerY = height * (1.0 - curveRoot.center);
                var maxY = height * (1.0 - curveRoot.maximum);

                ctx.beginPath();
                ctx.moveTo(0, minY);
                ctx.lineTo(centerX, centerY);
                ctx.lineTo(width, maxY);
                ctx.strokeStyle = "#f2f5f8";
                ctx.lineWidth = 3;
                ctx.stroke();

                ctx.fillStyle = "#1e88ff";
                ctx.beginPath();
                ctx.arc(0, minY, 5, 0, Math.PI * 2);
                ctx.fill();
                ctx.beginPath();
                ctx.arc(centerX, centerY, 6, 0, Math.PI * 2);
                ctx.fill();
                ctx.beginPath();
                ctx.arc(width, maxY, 5, 0, Math.PI * 2);
                ctx.fill();
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                property int activeHandle: 1

                function valueFromY(yValue) {
                    return root.clamp01(1.0 - yValue / Math.max(1, height));
                }

                function pickHandle(xValue) {
                    var centerDistance = Math.abs(xValue - width * 0.5);
                    var minimumDistance = Math.abs(xValue);
                    var maximumDistance = Math.abs(xValue - width);
                    if (minimumDistance <= centerDistance && minimumDistance <= maximumDistance)
                        return 0;
                    if (maximumDistance <= centerDistance)
                        return 2;
                    return 1;
                }

                function applyHandle(mouse) {
                    var nextValue = valueFromY(mouse.y);
                    if (activeHandle === 0)
                        root.setPressureCurve(nextValue, root.currentPressureCurveCenter, root.currentPressureCurveMaximum);
                    else if (activeHandle === 1)
                        root.setPressureCurve(root.currentPressureCurveMinimum, nextValue, root.currentPressureCurveMaximum);
                    else
                        root.setPressureCurve(root.currentPressureCurveMinimum, root.currentPressureCurveCenter, nextValue);
                }

                onPressed: function (mouse) {
                    activeHandle = pickHandle(mouse.x);
                    applyHandle(mouse);
                }
                onPositionChanged: function (mouse) {
                    if (pressed)
                        applyHandle(mouse);
                }
            }
        }

        Row {
            x: 0
            y: 96
            width: curveRoot.width
            height: 40
            spacing: 8

            CurveMiniSlider {
                objectName: "pressureCurveMinimumSlider"
                width: Math.max(1, (parent.width - 16) / 3)
                label: "Min"
                value: root.currentPressureCurveMinimum
                onMoved: function (value) {
                    root.setPressureCurve(value, root.currentPressureCurveCenter, root.currentPressureCurveMaximum);
                }
            }

            CurveMiniSlider {
                objectName: "pressureCurveCenterSlider"
                width: Math.max(1, (parent.width - 16) / 3)
                label: "Center"
                value: root.currentPressureCurveCenter
                onMoved: function (value) {
                    root.setPressureCurve(root.currentPressureCurveMinimum, value, root.currentPressureCurveMaximum);
                }
            }

            CurveMiniSlider {
                objectName: "pressureCurveMaximumSlider"
                width: Math.max(1, (parent.width - 16) / 3)
                label: "Max"
                value: root.currentPressureCurveMaximum
                onMoved: function (value) {
                    root.setPressureCurve(root.currentPressureCurveMinimum, root.currentPressureCurveCenter, value);
                }
            }
        }
    }

    component CurveMiniSlider: Item {
        id: miniRoot

        property string label: ""
        property real from: 0
        property real to: 1
        property alias value: slider.value

        signal moved(real value)

        height: 40

        LV.Label {
            x: 0
            y: 0
            width: miniRoot.width
            height: 14
            text: miniRoot.label
            style: caption
            color: "#8fa0b2"
            horizontalAlignment: Text.AlignHCenter
        }

        QC.Slider {
            id: slider
            x: 0
            y: 16
            width: miniRoot.width
            height: 22
            miniRoot.from
            to: miniRoot.to
            onMoved: miniRoot.moved(value)
        }
    }

    component BrushSlider: Item {
        id: sliderRoot

        property string label: ""
        property string valueText: ""
        property real from: 0
        property real to: 1
        property bool toggleVisible: false
        property bool toggleChecked: true
        property string toggleObjectName: ""
        property alias value: slider.value

        signal moved(real value)

        signal toggleChanged(bool checked)

        readonly property int toggleSlotWidth: toggleVisible ? 54 : 0

        height: 48

        LV.Label {
            x: 0
            y: 0
            width: Math.max(1, sliderRoot.width * 0.45)
            height: 18
            text: sliderRoot.label
            style: caption
            color: "#c7d2df"
        }

        LV.Label {
            x: Math.round(sliderRoot.width * 0.45)
            y: 0
            width: Math.max(1, sliderRoot.width - x - sliderRoot.toggleSlotWidth)
            height: 18
            text: sliderRoot.valueText
            style: caption
            color: "#8fa0b2"
            horizontalAlignment: Text.AlignRight
        }

        LV.ToggleSwitch {
            objectName: sliderRoot.toggleObjectName
            visible: sliderRoot.toggleVisible
            x: Math.round(sliderRoot.width - width)
            y: -2
            width: 42
            height: 22
            checked: sliderRoot.toggleChecked
            onCheckedChanged: sliderRoot.toggleChanged(checked)
        }

        QC.Slider {
            id: slider
            x: 0
            y: 22
            width: sliderRoot.width
            height: 24
            sliderRoot.from
            to: sliderRoot.to
            onMoved: sliderRoot.moved(value)
        }
    }
}
