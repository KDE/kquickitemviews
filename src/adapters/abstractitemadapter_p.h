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
#pragma once

// KQuickItemViews
#include "abstractitemadapter.h"
class ViewBase;
struct TreeTraversalItem;
class ViewItemContextAdapter;
class ContextAdapter;
class Viewport;
struct BlockMetadata;

// Qt
class QQuickItem;
#include <QtCore/QSharedPointer>

/**
 * Polymorphic tree item for the ViewBase.
 *
 * Classes implementing ViewBase need to provide an implementation of the pure
 * virtual functions. It is useful, for example, to manage both a raster and
 * QQuickItem based version of a view.
 *
 * The state is managed by the ViewBase and it's own protected virtual methods.
 */
class VisualTreeItem
{
    //FIXME remove all that
    friend class ViewBase;
    friend struct TreeTraversalItem;
    friend class ViewBasePrivate;
    friend class AbstractItemAdapterPrivate; //TODO remove
    friend class TreeTraversalReflector;
    friend class AbstractItemAdapter;
    friend class TreeTraversalReflectorPrivate; //TODO remove. debug only
    friend class ViewportPrivate; //TODO remove
public:
    using StateFlags = AbstractItemAdapter::StateFlags;

    explicit VisualTreeItem(ViewBase* p, Viewport* r) :
        m_pRange(r), m_pView(p) {}

    virtual ~VisualTreeItem() {}

    enum class State {
        POOLING , /*!< Being currently removed from view                      */
        POOLED  , /*!< Not currently in use, either new or waiting for re-use */
        BUFFER  , /*!< Not currently on screen, pre-loaded for performance    */
        ACTIVE  , /*!< Visible                                                */
        FAILED  , /*!< Loading the item was attempted, but failed             */
        DANGLING, /*!< Pending deletion, invalid pointers                     */
        ERROR   , /*!< Something went wrong                                   */
    };

    enum class ViewAction { //TODO make this private to ViewBasePrivate
        ATTACH       = 0, /*!< Activate the element (do not sync it) */
        ENTER_BUFFER = 1, /*!< Sync all roles                        */
        ENTER_VIEW   = 2, /*!< NOP (todo)                            */
        UPDATE       = 3, /*!< Reload the roles                      */
        MOVE         = 4, /*!< Move to a new position                */
        LEAVE_BUFFER = 5, /*!< Stop keeping track of data changes    */
        DETACH       = 6, /*!< Delete                                */
    };

    /// Call to notify that the geometry changed (for the selection delegate)
    void updateGeometry();

    void setCollapsed(bool v);
    bool isCollapsed() const;

    // Spacial navigation
    VisualTreeItem* up   (StateFlags flags = StateFlags::NORMAL) const;
    VisualTreeItem* down (StateFlags flags = StateFlags::NORMAL) const;
    VisualTreeItem* left (StateFlags flags = StateFlags::NORMAL) const { Q_UNUSED(flags); return nullptr ;}
    VisualTreeItem* right(StateFlags flags = StateFlags::NORMAL) const { Q_UNUSED(flags); return nullptr ;}
    int row   () const;
    int column() const;
    int depth() const;
    //TODO firstChild, lastChild, parent

    // Getters
    QPersistentModelIndex index() const;
    // Getters
    bool hasFailed() const{
        return m_State == State::FAILED;
    }

    /// Reference to the item own view
    ViewBase* view() const;

    /// Allow implementations to be notified when it becomes selected
    virtual void setSelected(bool) final;

    /// Geometry relative to the ViewBase::view()
    virtual QRectF geometry() const final;

    virtual QQuickItem* item() const final;

    QQmlContext *context() const;

    void setVisible(bool) {Q_ASSERT(false);}

    Viewport      *m_pRange    {nullptr};
    BlockMetadata *m_pGeometry {nullptr};

    bool performAction(ViewAction); //FIXME make private, remove #include

    AbstractItemAdapter* d_ptr;
private:
    State               m_State {State::POOLED};
    TreeTraversalItem  *m_pTTI  {   nullptr   };
    ViewBase           *m_pView {   nullptr   };

};
