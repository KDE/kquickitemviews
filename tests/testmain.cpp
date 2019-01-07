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
#include "freefloatingmodel.h"

#include <KQuickItemViews/plugin.h>

#ifdef KQUICKITEMVIEWS_USE_STATIC_PLUGIN
Q_IMPORT_PLUGIN(KQuickItemViews)
#else
#include <KQuickItemViews/plugin.h>
#endif


int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    qmlRegisterType<FreeFloatingModel>("modeltest", 1,0, "FreeFloatingModel");
    qmlRegisterType<ModelViewTester  >("modeltest", 1,0, "ModelViewTester");

#ifdef KQUICKITEMVIEWS_USE_STATIC_PLUGIN
      qobject_cast<QQmlExtensionPlugin*>(qt_static_plugin_KQuickItemViews().instance())->registerTypes("org.kde.playground.kquickitemviews");
#else
      KQuickItemViews v;
      v.registerTypes("org.kde.playground.kquickitemviews");
#endif

    engine.load(QUrl("qrc:///modeltest.qml"));

    qDebug() << "LOADED" << engine.rootObjects() << engine.rootObjects().isEmpty();

    return engine.rootObjects().isEmpty() ? 1 : app.exec();
}
