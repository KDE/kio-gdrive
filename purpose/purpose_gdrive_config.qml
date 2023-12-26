/*
    SPDX-FileCopyrightText: 2020 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

import QtQuick 2.2
import QtQuick.Controls 2.10
import QtQuick.Layouts 1.1
import org.kde.kirigami 2.12 as Kirigami
import org.kde.kquickcontrolsaddons 2.0 as KQCA
import SSO.OnlineAccounts 0.1 as OA

ColumnLayout
{
    id: root

    property var folder: folderField.text
    property var accountName
    property var urls
    property var mimeType

    Kirigami.Heading {
        text: i18nd("kio5_gdrive", "Select an account:")
        visible: list.count !== 0
    }

    ScrollView {
        id: scroll

        Layout.fillWidth: true
        Layout.fillHeight: true

        Component.onCompleted: scroll.background.visible = true

        ListView {
            id: list

            clip: true

            model: OA.AccountServiceModel {
                id: serviceModel
                serviceType: "google-drive"
            }

            delegate: Kirigami.BasicListItem {
                text: model.displayName
            }

            onCurrentIndexChanged: {
                if (currentIndex === -1) {
                    root.accountName = undefined
                    return
                }

                root.accountName = serviceModel.get(list.currentIndex, "displayName")
            }

            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                width: parent.width - (Kirigami.Units.largeSpacing * 4)
                visible: list.count === 0
                text: i18nd("kio5_gdrive", "No account configured")
            }
        }
    }

    Button {
        Layout.alignment: Qt.AlignRight

        text: i18nd("kio5_gdrive", "Configure Accounts")
        icon.name: "applications-internet"
        onClicked: KQCA.KCMShell.openSystemSettings("kcm_kaccounts")
    }

    Label {
        Layout.fillWidth: true
        text: i18nd("kio5_gdrive", "Upload to folder:")
    }

    TextField {
        id: folderField
        Layout.fillWidth: true
        text: "/"
        onTextChanged: {
            // Setting folder to undefined disables the Run button
            root.folder = text !== "" ? text : undefined
        }
    }
}
