pragma Singleton
pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import Quickshell.Io

Singleton {
    id: root

    property int users: 0
    property var frame: ({ levels: [], colors: [], height: 0.48, width: 0.72, opacity: 0.60, lockscreen: true })

    Process {
        id: reader
        command: [Quickshell.env("HOME") + "/.local/bin/hypr-visualizer", "--levels"]
        running: root.users > 0
        onRunningChanged: {
            if (!running)
                root.frame = { levels: [], colors: [], height: 0.48, width: 0.72, opacity: 0.60, lockscreen: true };
        }
        stdout: SplitParser {
            onRead: data => {
                try {
                    root.frame = JSON.parse(data);
                } catch (error) {
                    console.warn("hypr-visualizer: invalid spectrum frame");
                }
            }
        }
    }

    Timer {
        interval: 2000
        repeat: true
        running: root.users > 0
        onTriggered: {
            if (!reader.running)
                reader.running = true;
        }
    }
}
