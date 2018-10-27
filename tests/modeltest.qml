/***************************************************************************
 *   Copyright (C) 2017 by Emmanuel Lepage Vallee                          *
 *   Author : Emmanuel Lepage Vallee <elv1313@gmail.com>                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/
import QtQuick.Window 2.2
import QtQuick 2.7
import QtQuick.Layouts 1.0
import RingQmlWidgets 1.0
import QtQuick.Controls 2.0
import org.kde.kirigami 2.2 as Kirigami

Kirigami.ApplicationWindow {
    id: window
    visible: true

    globalDrawer: Kirigami.GlobalDrawer {
        id: globalDrawer
        title: "KQuickItemViews"
        bannerImageSource: ""
        actions: [
            Kirigami.Action {
                iconName: "document-edit"
                text: "HierarchyView"
                onTriggered: {
                    listview.model = treeTester
                    treeTester.run()
                }
            },
            Kirigami.Action {
                iconName: "document-edit"
                text: "HierarchyView (list)"
                onTriggered: {
                    listview.model = treeTester2
                    treeTester2.run()
                }
            },
            Kirigami.Action {
                iconName: "document-edit"
                text: "TreeView"
                onTriggered: {}
            },
            Kirigami.Action {
                iconName: "document-edit"
                text: "ListView"
                onTriggered: {}
            }
        ]
        handleVisible: true
        drawerOpen: false
    }

    ModelViewTester {
        id: treeTester
    }

    ListModelTester {
        id: treeTester2
    }

    ColumnLayout {
        anchors.fill: parent

        Slider {
            Layout.fillWidth: true
            from: 0
            to: 5000
            onValueChanged: treeTester.interval = value
        }

        QuickTreeView {
            id: listview
            Layout.fillHeight: true
            Layout.fillWidth: true
            model: testmodel
            delegate: Rectangle {
                anchors.leftMargin: offset
                opacity: 0.7
                height: 20
                width: listview.width
                color: "blue"
                Text {
                    anchors.leftMargin: offset
                    anchors.fill: parent
                    text: display
                    color: "red"
                }
            }
        }
    }
}
