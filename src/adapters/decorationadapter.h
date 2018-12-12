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
#ifndef KQUICKITEMVIEWS_DECORATIONADAPTER_H
#define KQUICKITEMVIEWS_DECORATIONADAPTER_H

// Qt
#include <QQuickPaintedItem>
#include <QtGui/QPixmap>
#include <QtGui/QIcon>

class DecorationAdapterPrivate;

class DecorationAdapter : public QQuickPaintedItem
{
   Q_OBJECT
   Q_PROPERTY(QVariant pixmap READ pixmap WRITE setPixmap NOTIFY changed)
   Q_PROPERTY(QString themeFallback READ themeFallback WRITE setThemeFallback NOTIFY changed)
   Q_PROPERTY(bool hasPixmap READ hasPixmap NOTIFY changed)

public:
    explicit DecorationAdapter(QQuickItem* parent = nullptr);
    virtual ~DecorationAdapter();

    QPixmap pixmap() const;
    void setPixmap(const QVariant& var);

    QString themeFallback() const;
    void setThemeFallback(const QString& s);

    bool hasPixmap() const;

    virtual void paint(QPainter *painter) override;

Q_SIGNALS:
    void changed();

private:
    DecorationAdapterPrivate *d_ptr;
    Q_DECLARE_PRIVATE(DecorationAdapter)
};

#endif
