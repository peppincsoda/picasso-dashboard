
import QtQuick 2.2
import QtQuick.Window 2.1
import QtQuick.Extras 1.4
import app 1.0

Window {
    id: root
    visible: true
    visibility: "FullScreen"
    width: 600
    height: 480
    color: "black"

    Item {
        anchors.fill: parent

        CircularGauge {
            id: rpmGauge
            value: appContext.rpmValue / 100

            width: Math.min(root.width, root.height)
            height: width
            anchors.centerIn: parent

            Text {
                text: "RPM<br>x100"
                color: "white"
                font.pixelSize: 48
                anchors.bottom: parent.bottom
                anchors.bottomMargin: parent.height * 0.2
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }

        MessageBox {
            id: messageBox
            width: parent.width * 0.5
            height: parent.height * 0.25
            anchors.centerIn: parent
            visible: false
        }

        Text {
            id: fpsLabel
            text: "FPS: " + appContext.fpsValue
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.top: parent.top
            anchors.topMargin: 10
            font.family: "Verdana"
            font.pixelSize: 24
            color: "yellow"
        }

        states: [
            State {
                name: "showingMessage"
                when: appContext.message != ""
                PropertyChanges {
                    target: messageBox
                    text: appContext.message
                    visible: true
                }
            }
        ]
    }

    AppContext {
        id: appContext
    }

    Component.onCompleted: {
        appContext.start()
    }
}
