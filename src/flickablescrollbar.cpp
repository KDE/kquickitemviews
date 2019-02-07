/***************************************************************************
 *   Copyright (C) 2017-2018 by Emmanuel Lepage Vallee                     *
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
#include "flickablescrollbar.h"

// KQuickItemViews
#include "views/flickable.h"

class FlickableScrollBarPrivate : public QObject
{
    Q_OBJECT
public:
    Flickable *m_pView        {nullptr};
    qreal      m_HandleHeight {   0   };
    qreal      m_Position     {   0   };
    bool       m_Visible      { false };

    FlickableScrollBar* q_ptr;

public Q_SLOTS:
    void recomputeGeometry();
};

FlickableScrollBar::FlickableScrollBar(QQuickItem* parent) : QQuickItem(parent),
    d_ptr(new FlickableScrollBarPrivate)
{
    d_ptr->q_ptr = this;
}

FlickableScrollBar::~FlickableScrollBar()
{
    delete d_ptr;
}

QObject* FlickableScrollBar::view() const
{
    return d_ptr->m_pView;
}

void FlickableScrollBar::setView(QObject* v)
{
    if (d_ptr->m_pView) {
        disconnect(d_ptr->m_pView, &Flickable::contentHeightChanged,
            d_ptr, &FlickableScrollBarPrivate::recomputeGeometry);
        disconnect(d_ptr->m_pView, &Flickable::contentYChanged,
            d_ptr, &FlickableScrollBarPrivate::recomputeGeometry);
        disconnect(d_ptr->m_pView, &Flickable::heightChanged,
            d_ptr, &FlickableScrollBarPrivate::recomputeGeometry);
    }

    d_ptr->m_pView = qobject_cast<Flickable*>(v);

    Q_ASSERT((!v) || d_ptr->m_pView);

    connect(d_ptr->m_pView, &Flickable::contentHeightChanged,
        d_ptr, &FlickableScrollBarPrivate::recomputeGeometry);
    connect(d_ptr->m_pView, &Flickable::contentYChanged,
        d_ptr, &FlickableScrollBarPrivate::recomputeGeometry);
    connect(d_ptr->m_pView, &Flickable::heightChanged,
        d_ptr, &FlickableScrollBarPrivate::recomputeGeometry);

    d_ptr->recomputeGeometry();
}

qreal FlickableScrollBar::position() const
{
    return d_ptr->m_Position;
}

void FlickableScrollBar::setPosition(qreal p)
{
    if (!d_ptr->m_pView)
        return;

    // Simple rule of 3
    d_ptr->m_pView->setContentY(
        (p * d_ptr->m_pView->contentHeight()) / d_ptr->m_pView->height()
    );
}

qreal FlickableScrollBar::handleHeight() const
{
    return d_ptr->m_HandleHeight;
}

bool FlickableScrollBar::isHandleVisible() const
{
    return d_ptr->m_Visible;
}

/**
 * The idea behind the scrollhandle is that the height represent the height of a
 * page until it gets too small. In mobile mode, the height is always the same
 * regardless of the content height to hide less space on the smaller screen.
 */
void FlickableScrollBarPrivate::recomputeGeometry()
{
    if (!m_pView)
        return;

    const qreal oldP = m_Position;
    const qreal oldH = m_HandleHeight;

    const qreal totalHeight  = m_pView->contentHeight();
    const qreal pageHeight   = m_pView->height();
    const qreal pageCount    = totalHeight/pageHeight;
    const qreal handleHeight = std::max(pageHeight/pageCount, 50.0);
    const qreal handleBegin  = (m_pView->contentY()*pageHeight)/totalHeight;

    m_HandleHeight = handleHeight;
    m_Position     = handleBegin;
    m_Visible      = totalHeight > pageHeight;

    if (oldH != m_HandleHeight)
        emit q_ptr->handleHeightChanged();

    if (oldP != m_Position)
        emit q_ptr->positionChanged ();
}

#include <flickablescrollbar.moc>
