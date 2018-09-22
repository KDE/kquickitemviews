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
#include "decorationadapter.h"

// Qt
#include <QtGui/QPainter>

class DecorationAdapterPrivate
{
public:
    QPixmap m_Pixmap;
    QIcon   m_Icon  ;
    QString m_ThemeFallback;
    QIcon   m_FallbackIcon;
};

DecorationAdapter::DecorationAdapter(QQuickItem* parent) : QQuickPaintedItem(parent),
    d_ptr(new DecorationAdapterPrivate())
{}

DecorationAdapter::~DecorationAdapter()
{
    delete d_ptr;
}

QPixmap DecorationAdapter::pixmap() const {
    return d_ptr->m_Pixmap;
}

void DecorationAdapter::setPixmap(const QVariant& var)
{
    d_ptr->m_Pixmap = qvariant_cast<QPixmap>(var);
    d_ptr->m_Icon   = qvariant_cast<QIcon  >(var);
    update();
    emit changed();
}

void DecorationAdapter::paint(QPainter *painter)
{
    if (!d_ptr->m_Icon.isNull()) {
        const QPixmap pxm = d_ptr->m_Icon.pixmap(boundingRect().size().toSize());

        painter->drawPixmap(
            boundingRect().toRect(),
            pxm,
            pxm.rect()
        );
    }
    else if (!d_ptr->m_Pixmap.isNull()) {
        painter->drawPixmap(
            boundingRect(),
            d_ptr->m_Pixmap,
            d_ptr->m_Pixmap.rect()
        );
    }
    else if (!d_ptr->m_ThemeFallback.isEmpty()) {
        if (d_ptr->m_FallbackIcon.isNull())
            d_ptr->m_FallbackIcon = QIcon::fromTheme(d_ptr->m_ThemeFallback);

        const QPixmap pxm = d_ptr->m_FallbackIcon.pixmap(boundingRect().size().toSize());

        painter->drawPixmap(
            boundingRect().toRect(),
            pxm,
            pxm.rect()
        );
    }
}

QString DecorationAdapter::themeFallback() const
{
    return d_ptr->m_ThemeFallback;
}

void DecorationAdapter::setThemeFallback(const QString& s)
{
    d_ptr->m_ThemeFallback = s;
    d_ptr->m_FallbackIcon  = {};
    update();
    emit changed();
}

bool DecorationAdapter::hasPixmap() const
{
    qDebug() << d_ptr->m_FallbackIcon.isNull() << d_ptr->m_Pixmap.isNull() << d_ptr->m_Icon.isNull();
    return !(d_ptr->m_FallbackIcon.isNull() && d_ptr->m_Pixmap.isNull() && d_ptr->m_Icon.isNull());
}
