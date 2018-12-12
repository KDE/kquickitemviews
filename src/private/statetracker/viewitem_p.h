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
#ifndef KQUICKITEMVIEWS_VIEWITEM_P_H
#define KQUICKITEMVIEWS_VIEWITEM_P_H

// KQuickItemViews
#include "private/statetracker/viewitem_p.h"
#include <adapters/abstractitemadapter.h>
#include <private/indexmetadata_p.h>
class ViewBase;
class ViewItemContextAdapter;
class ContextAdapter;
class Viewport;

// Qt
class QQuickItem;
#include <QtCore/QSharedPointer>

namespace StateTracker {

class Content;

/**
 * Polymorphic tree item for the ViewBase.
 *
 * Classes implementing ViewBase need to provide an implementation of the pure
 * virtual functions. It is useful, for example, to manage both a raster and
 * QQuickItem based version of a view.
 *
 * The state is managed by the ViewBase and it's own protected virtual methods.
 */
class ViewItem
{
public:
    explicit ViewItem(Viewport* r) :
        m_pViewport(r) {}

    virtual ~ViewItem() {
        m_pMetadata->setViewTracker(nullptr);
    }

    enum class State {
        POOLING , /*!< Being currently removed from view                      */
        POOLED  , /*!< Not currently in use, either new or waiting for re-use */
        BUFFER  , /*!< Not currently on screen, pre-loaded for performance    */
        ACTIVE  , /*!< Visible                                                */
        FAILED  , /*!< Loading the item was attempted, but failed             */
        DANGLING, /*!< Pending deletion, invalid pointers                     */
        ERROR   , /*!< Something went wrong                                   */
    };

    /// Call to notify that the geometry changed (for the selection delegate)
    void updateGeometry();

    void setCollapsed(bool v);
    bool isCollapsed() const;

    // Spacial navigation
    ViewItem* up   () const; //DEPRECATED
    ViewItem* down () const; //DEPRECATED
    ViewItem* left () const { return nullptr ;} //DEPRECATED
    ViewItem* right() const { return nullptr ;} //DEPRECATED
    ViewItem *next(Qt::Edge e) const;

    int row   () const;
    int column() const;
    int depth () const;
    //TODO firstChild, lastChild, parent

    // Getters
    QPersistentModelIndex index() const;

    /// Allow implementations to be notified when it becomes selected
    virtual void setSelected(bool) final;

    /// Geometry relative to the ViewBase::view()
    virtual QRectF currentGeometry() const final;

    virtual QQuickItem* item() const final;

    QQmlContext *context() const;

    Viewport      *m_pViewport {nullptr};
    IndexMetadata *m_pMetadata {nullptr};

    bool performAction(IndexMetadata::ViewAction a);

    State state() const;

    AbstractItemAdapter* d_ptr;
private:
    State m_State {State::POOLED};
};

}

#endif
