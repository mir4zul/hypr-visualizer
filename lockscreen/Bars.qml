pragma ComponentBehavior: Bound

import QtQuick
import "."

Item {
    id: root

    clip: true
    opacity: Levels.frame.opacity
    readonly property var frame: Levels.frame
    readonly property int count: frame.colors.length
    readonly property real step: width / Math.max(1, count)

    Component.onCompleted: Levels.users++
    Component.onDestruction: Levels.users--

    Repeater {
        model: root.count
        delegate: Rectangle {
            id: bar
            required property int index
            readonly property real level: root.frame.levels[index] || 0
            width: root.step * root.frame.width
            height: level * Math.max(0, root.height * root.frame.height - width) + width
            x: (index + 0.5) * root.step - width / 2
            y: root.height - height + width / 2
            radius: width / 2
            readonly property color barColor: root.frame.colors[index]
            readonly property real visibleEnd: (height - width / 2) / Math.max(1, height)
            gradient: Gradient {
                GradientStop { position: 0; color: bar.barColor }
                GradientStop { position: 0.65 * bar.visibleEnd; color: bar.barColor }
                GradientStop {
                    position: bar.visibleEnd
                    color: Qt.rgba(bar.barColor.r, bar.barColor.g, bar.barColor.b, root.frame.baseAlpha ?? 0.22)
                }
                GradientStop {
                    position: 1
                    color: Qt.rgba(bar.barColor.r, bar.barColor.g, bar.barColor.b, root.frame.baseAlpha ?? 0.22)
                }
            }
            visible: level * root.height >= 1
        }
    }
}
