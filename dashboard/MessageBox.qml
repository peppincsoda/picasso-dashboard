
import QtQuick 2.2

Item {
    id: container
    property alias text: message.text
    width: 600
    height: 300

    Rectangle {
        id: rect
        anchors.fill: parent
        anchors.margins: 5
        color: "lightblue"
        border.color: "black"

        Text {
            id: message
            anchors.fill: parent
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter

            font.family: "Verdana"
            font.pixelSize: 24
        }
    }
}
