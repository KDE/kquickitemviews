/***************************************************************************
 *   Copyright (C) 2017 by Emmanuel Lepage Vallee                          *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@kde.org>             *
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

//Qt
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QQuickView>
#include <QQmlEngine>
#include <QQmlContext>
#include <QSharedPointer>
#include <QQmlApplicationEngine>

#include "modelviewtester.h"

#include <KQuickItemViews/views/hierarchyview.h>
#include <KQuickItemViews/views/listview.h>
#include <KQuickItemViews/views/treeview.h>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    qmlRegisterType<ModelViewTester>("RingQmlWidgets", 1, 0, "ModelViewTester");
    qmlRegisterType<HierarchyView  >("RingQmlWidgets", 1, 0, "HierarchyView");
    qmlRegisterType<TreeView       >("RingQmlWidgets", 1, 0, "QuickTreeView");
    qmlRegisterType<ListView       >("RingQmlWidgets", 1, 0, "QuickListView");

//     view.setResizeMode(QQuickView::SizeRootObjectToView);
    engine.load(QUrl("qrc:///modeltest.qml"));

    if (engine.rootObjects().isEmpty()) {
        qDebug() << "\n\nFAILED TO LOAD";
        return -1;
    }




    return app.exec();
}

#include <testmain.moc>
