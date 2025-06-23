/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2014 Sebastian Kügler <sebas@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.plasma.components 3.0 as PlasmaComponents3

GridLayout {
    id: toolButtonsLayout

    enum ButtonRole {
        ToggleStar,
        InvokeAction,
        ShowQRCode,
        Edit,
        Remove
    }

    required property PlasmaComponents3.ItemDelegate menuItem
    required property bool shouldUseOverflowButton

    readonly property var buttonDefinitions: [
        {
            role: DelegateToolButtons.ButtonRole.ToggleStar,
            // Binding directly references menuItem's state with safety check
            icon: (menuItem.model?.starred ?? false) ? "starred-symbolic" : "non-starred-symbolic",
            // Binding directly references menuItem's state with safety check
            text: (menuItem.model?.starred ?? false) ? i18nd("klipper", "Remove Star") : i18nd("klipper", "Star"),
            // visible: true // Optional, defaults to true
        },
        {
            role: DelegateToolButtons.ButtonRole.InvokeAction,
            icon: "system-run", // Static value
            text: i18nd("klipper", "Invoke action"), // Static value
            // visible: true
        },
        {
            role: DelegateToolButtons.ButtonRole.ShowQRCode,
            icon: "view-barcode-qr", // Static value
            text: i18nd("klipper", "Show QR code"), // Static value
            // visible: true
        },
        {
            role: DelegateToolButtons.ButtonRole.Edit,
            icon: "document-edit", // Static value
            text: i18nd("klipper", "Edit contents"), // Static value
            // Binding directly references menuItem's state
            visible: menuItem.type === 2 // Only show for text items (assuming type 2 is text)
        },
        {
            role: DelegateToolButtons.ButtonRole.Remove,
            icon: "edit-delete", // Static value
            text: i18nd("klipper", "Remove from history"), // Static value
            // visible: true
        }
    ]

    readonly property Item defaultButton: visibleChildren.length > 0 ? visibleChildren[0] : this
    // https://bugreports.qt.io/browse/QTBUG-108821
    readonly property bool hovered: visibleChildren.filter(x => x.hovered).length > 0

    rows: shouldUseOverflowButton ? (buttonDefinitions.length - (menuItem.type === 2 ? 0 : 1)) : 1
    columns: shouldUseOverflowButton ? 1 : (buttonDefinitions.length - (menuItem.type === 2 ? 0 : 1))
    rowSpacing: Kirigami.Units.smallSpacing
    columnSpacing: Kirigami.Units.smallSpacing

    function trigger(actionRole: int): void {
        switch (actionRole) {
            case DelegateToolButtons.ButtonRole.ToggleStar:
            if (menuItem.model) {
                menuItem.model.starred = !(menuItem.model?.starred ?? false);
            }
            break;
        case DelegateToolButtons.ButtonRole.InvokeAction:
            menuItem.triggerAction();
            break;
        case DelegateToolButtons.ButtonRole.ShowQRCode:
            menuItem.barcode();
            break;
        case DelegateToolButtons.ButtonRole.Edit:
            menuItem.edit();
            break;
        case DelegateToolButtons.ButtonRole.Remove:
            menuItem.remove();
            break;
        }
    }

    Repeater {
        id: repeater
        model: toolButtonsLayout.buttonDefinitions

        PlasmaComponents3.ToolButton {
            required property int index
            required property var modelData
            
            Layout.fillWidth: toolButtonsLayout.shouldUseOverflowButton
            Layout.leftMargin: toolButtonsLayout.shouldUseOverflowButton ? Kirigami.Units.gridUnit : 0
            Layout.rightMargin: toolButtonsLayout.shouldUseOverflowButton ? Kirigami.Units.gridUnit : 0

            display: toolButtonsLayout.shouldUseOverflowButton ? PlasmaComponents3.AbstractButton.TextBesideIcon : PlasmaComponents3.AbstractButton.IconOnly
            text: modelData.text
            icon.name: modelData.icon
            visible: modelData.visible ?? true

            KeyNavigation.right: (index === repeater.count - 1 ? this : repeater.itemAt(index + 1)) as PlasmaComponents3.ToolButton
            PlasmaComponents3.ToolTip.text: text
            PlasmaComponents3.ToolTip.delay: Kirigami.Units.toolTipDelay
            PlasmaComponents3.ToolTip.visible: hovered || (activeFocus && (focusReason === Qt.TabFocusReason || focusReason === Qt.BacktabFocusReason))
            onClicked: toolButtonsLayout.trigger(modelData.role)
        }
    }
}
