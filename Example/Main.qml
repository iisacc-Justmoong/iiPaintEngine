pragma
ComponentBehavior: Bound
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
        {"name": "Ink", "color": "#101318"},
        {"name": "Lake", "color": "#2563eb"},
        {"name": "Carmine", "color": "#d7264c"},
        {"name": "Moss", "color": "#2f7d52"},
        {"name": "Amber", "color": "#f5a524"},
        {"name": "Violet", "color": "#7c3aed"}
    ]
    readonly property bool demoReady: demoCanvas.width > 0
        && demoCanvas.height > 0
        && paintControls.width > 0
        && clearButton !== null
        && livePreviewToggle !== null

    property int activeSwatchIndex: 0
    property color activeBrushColor: swatches[activeSwatchIndex].color
    property real currentBrushSize: 18.0
    property real currentFlow: 0.78
    property real currentOpacity: 0.92
    property real currentHardness: 0.72
    property real currentSpacingRatio: 0.22
    property real currentZoom: 1.0

    function applyBrushSettings() {
        demoCanvas.setBrush(currentBrushSize, activeBrushColor, currentFlow, currentOpacity)
        demoCanvas.brushHardness = currentHardness
        demoCanvas.brushSpacingRatio = currentSpacingRatio
        demoCanvas.livePreviewEnabled = livePreviewToggle.checked
    }

    function chooseSwatch(index) {
        if (index < 0 || index >= swatches.length)
            return
        activeSwatchIndex = index
        activeBrushColor = swatches[index].color
        applyBrushSettings()
    }

    function setZoom(value) {
        currentZoom = Math.max(0.25, Math.min(3.0, value))
        demoCanvas.zoom = currentZoom
    }

    Component.onCompleted: {
        demoCanvas.resetView()
        applyBrushSettings()
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
                    root.setZoom(1.0)
                    demoCanvas.resetView()
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
                width: parent.width
                label: "Size"
                valueText: Math.round(root.currentBrushSize) + " px"
                2
                to: 72
                value: root.currentBrushSize
                onMoved: function (value) {
                    root.currentBrushSize = value
                    root.applyBrushSettings()
                }
            }

            BrushSlider {
                width: parent.width
                label: "Flow"
                valueText: Math.round(root.currentFlow * 100) + "%"
                0.05
                to: 1
                value: root.currentFlow
                onMoved: function (value) {
                    root.currentFlow = value
                    root.applyBrushSettings()
                }
            }

            BrushSlider {
                width: parent.width
                label: "Opacity"
                valueText: Math.round(root.currentOpacity * 100) + "%"
                0.05
                to: 1
                value: root.currentOpacity
                onMoved: function (value) {
                    root.currentOpacity = value
                    root.applyBrushSettings()
                }
            }

            BrushSlider {
                width: parent.width
                label: "Hardness"
                valueText: Math.round(root.currentHardness * 100) + "%"
                0.05
                to: 1
                value: root.currentHardness
                onMoved: function (value) {
                    root.currentHardness = value
                    root.applyBrushSettings()
                }
            }

            BrushSlider {
                width: parent.width
                label: "Spacing"
                valueText: Math.round(root.currentSpacingRatio * 100) + "%"
                0.05
                to: 0.8
                value: root.currentSpacingRatio
                onMoved: function (value) {
                    root.currentSpacingRatio = value
                    root.applyBrushSettings()
                }
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
            brushOpacity: root.currentOpacity
            brushHardness: root.currentHardness
            brushSpacingRatio: root.currentSpacingRatio
            livePreviewEnabled: livePreviewToggle.checked
            multithreadedEventsEnabled: true
        }
    }

    component BrushSlider: Item {
        id: sliderRoot

        property string label: ""
        property string valueText: ""
        property real from: 0
        property real to: 1
        property alias value: slider.value

        signal moved(real value)

        height: 46

        LV.Label {
            x: 0
            y: 0
            width: sliderRoot.width * 0.55
            height: 18
            text: sliderRoot.label
            style: caption
            color: "#c7d2df"
        }

        LV.Label {
            x: sliderRoot.width * 0.55
            y: 0
            width: sliderRoot.width * 0.45
            height: 18
            text: sliderRoot.valueText
            style: caption
            color: "#8fa0b2"
            horizontalAlignment: Text.AlignRight
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
