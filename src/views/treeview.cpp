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
#include "treeview.h"

// KQuickItemViews
#include "adapters/abstractitemadapter.h"
#include "extensions/contextextension.h"
#include "adapters/modeladapter.h"
#include "contextadapterfactory.h"
#include "adapters/contextadapter.h"

// Qt
#include <QQmlContext>

/**
 * Polymorphic tree item for the SingleModelViewBase.
 *
 * Classes implementing SingleModelViewBase need to provide an implementation of the pure
 * virtual functions. It is useful, for example, to manage both a raster and
 * QQuickItem based version of a view.
 *
 * The state is managed by the SingleModelViewBase and it's own protected virtual methods.
 */
class TreeViewItem final : public AbstractItemAdapter
{
public:
    explicit TreeViewItem(Viewport* r);
    virtual ~TreeViewItem();

    // Actions
    virtual bool move   () override;
    virtual bool remove () override;

private:
    bool m_IsHead { false };
};

/// Add the same property as the QtQuick.ListView
class TreeContextProperties final : public ContextExtension
{
public:
    virtual ~TreeContextProperties() {}
    virtual QVector<QByteArray>& propertyNames() const override;
    virtual QVariant getProperty(AbstractItemAdapter* item, uint id, const QModelIndex& index) const override;
    virtual bool setProperty(AbstractItemAdapter* item, uint id, const QVariant& value, const QModelIndex& index) const override;
};

class TreeViewPrivate
{
public:
};

TreeView::TreeView(QQuickItem* parent) : SingleModelViewBase(new ItemFactory<TreeViewItem>(), parent),
    d_ptr(new TreeViewPrivate)
{
    modelAdapters().first()->contextAdapterFactory()->addContextExtension(
        new TreeContextProperties()
    );
}

TreeView::~TreeView()
{
    delete d_ptr;
}

TreeViewItem::TreeViewItem(Viewport* r) : AbstractItemAdapter(r)
{
}

TreeViewItem::~TreeViewItem()
{
    delete item();
}

bool TreeViewItem::move()
{
    // Will happen when trying to move a FAILED, but buffered item
    if (!item()) {
        qDebug() << "NO ITEM" << index().data();
        return false;
    }

    item()->setWidth(view()->contentItem()->width());

    auto nextElem = static_cast<TreeViewItem*>(next(Qt::BottomEdge));
    auto prevElem = static_cast<TreeViewItem*>(next(Qt::TopEdge));

    // The root has been moved in the middle of the tree, find the new root
    //TODO maybe add a deterministic API instead of O(N) lookup
    if (prevElem && m_IsHead) {
        m_IsHead = false;

        auto root = prevElem;
        while (auto prev = root->next(Qt::TopEdge))
            root = static_cast<TreeViewItem*>(prev);

        root->move();
        Q_ASSERT(root->m_IsHead);
    }

    // So other items can be GCed without always resetting to 0x0, note that it
    // might be a good idea to extend Flickable to support a virtual
    // origin point.
    if ((!prevElem) || (nextElem && nextElem->m_IsHead)) {
        auto anchors = qvariant_cast<QObject*>(item()->property("anchors"));
        anchors->setProperty("top", {});
        item()->setY(0);
        m_IsHead = true;
    }
    else if (prevElem) {
        Q_ASSERT(!m_IsHead);
        item()->setProperty("y", {});
        auto anchors = qvariant_cast<QObject*>(item()->property("anchors"));
        anchors->setProperty("top", prevElem->item()->property("bottom"));
    }

    // Now, update the next anchors
    if (nextElem) {
        nextElem->m_IsHead = false;
        nextElem->item()->setProperty("y", {});

        auto anchors = qvariant_cast<QObject*>(nextElem->item()->property("anchors"));
        anchors->setProperty("top", item()->property("bottom"));
    }

    updateGeometry();

    return true;
}

bool TreeViewItem::remove()
{
    if (item()) {
        item()->setParent(nullptr);
        item()->setParentItem(nullptr);
        item()->setVisible(false);
    }

    auto nextElem = static_cast<TreeViewItem*>(next(Qt::BottomEdge));
    auto prevElem = static_cast<TreeViewItem*>(next(Qt::TopEdge));

    if (nextElem) {
        if (m_IsHead) {
            auto anchors = qvariant_cast<QObject*>(nextElem->item()->property("anchors"));
            anchors->setProperty("top", {});
            item()->setY(0);
            nextElem->m_IsHead = true;
        }
        else { //TODO maybe eventually use a state machine for this
            auto anchors = qvariant_cast<QObject*>(nextElem->item()->property("anchors"));
            anchors->setProperty("top", prevElem->item()->property("bottom"));
        }
    }

    return true;
}

QVector<QByteArray>& TreeContextProperties::propertyNames() const
{
    static QVector<QByteArray> ret { "expanded" };
    return ret;
}

QVariant TreeContextProperties::getProperty(AbstractItemAdapter* item, uint id, const QModelIndex& index) const
{
    Q_UNUSED(index);
    Q_ASSERT(id == 0 && item);
    return !item->isCollapsed();
}

bool TreeContextProperties::setProperty(AbstractItemAdapter* item, uint id, const QVariant& value, const QModelIndex& index) const
{
    Q_ASSERT(id == 0 && item && value.canConvert<bool>());
    item->setCollapsed(!value.toBool());
    return true;
}
