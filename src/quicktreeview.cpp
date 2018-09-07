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
#include "quicktreeview.h"

#include "abstractviewitem.h"

// Qt
#include <QQmlContext>

/**
 * Polymorphic tree item for the AbstractViewCompat.
 *
 * Classes implementing AbstractViewCompat need to provide an implementation of the pure
 * virtual functions. It is useful, for example, to manage both a raster and
 * QQuickItem based version of a view.
 *
 * The state is managed by the AbstractViewCompat and it's own protected virtual methods.
 */
class QuickTreeViewItem : public AbstractViewItem
{
public:
    explicit QuickTreeViewItem(AbstractViewCompat* v);
    virtual ~QuickTreeViewItem();

    // Actions
    virtual bool attach () override;
    virtual bool refresh() override;
    virtual bool move   () override;
    virtual bool flush  () override;
    virtual bool remove () override;

    virtual void setSelected(bool s) final override;

//     virtual QQuickItem* item() const final override {
//         return qvariant_cast<QQuickItem*>(
//             item()->property("content")
//         );
//     }

private:
    bool m_IsHead { false };

    QuickTreeViewPrivate* d() const;
};

class QuickTreeViewPrivate
{
public:

    // When all elements are assumed to have the same height, life is easy
    QVector<qreal> m_DepthChart {0};

    QuickTreeView* q_ptr;
};

QuickTreeView::QuickTreeView(QQuickItem* parent) : AbstractViewCompat(parent),
    d_ptr(new QuickTreeViewPrivate)
{
    d_ptr->q_ptr = this;
}

QuickTreeView::~QuickTreeView()
{
    delete d_ptr;
}

AbstractViewItem* QuickTreeView::createItem() const
{
    return new QuickTreeViewItem(
        const_cast<QuickTreeView*>(this)
    );
}

QuickTreeViewItem::QuickTreeViewItem(AbstractViewCompat* p) : AbstractViewItem(p)
{
}

QuickTreeViewItem::~QuickTreeViewItem()
{
    delete item();
}

QuickTreeViewPrivate* QuickTreeViewItem::d() const
{
    return static_cast<QuickTreeView*>(view())->QuickTreeView::d_ptr;
}

bool QuickTreeViewItem::attach()
{
    // This will trigger the lazy-loading of the item
    if (!item())
        return false;

    /*d()->m_DepthChart[depth()] = std::max(
        d()->m_DepthChart[depth()],
        pair.first->height()
    );*/

    Q_ASSERT(index().isValid());

    // Add some useful metadata
    context()->setContextProperty("rowCount", index().model()->rowCount(index()));
    context()->setContextProperty("index", index().row());
    context()->setContextProperty("modelIndex", index());

    Q_ASSERT(item() && context());

    return move();
}

bool QuickTreeViewItem::refresh()
{
    if (context())
        d()->q_ptr->applyRoles(context(), index());

    return true;
}

bool QuickTreeViewItem::move()
{
    // Will happen when trying to move a FAILED, but buffered item
    if (!item()) {
        qDebug() << "NO ITEM" << index().data();
        return false;
    }

    item()->setWidth(view()->contentItem()->width());

    auto nextElem = static_cast<QuickTreeViewItem*>(down());
    auto prevElem = static_cast<QuickTreeViewItem*>(up());

    // The root has been moved in the middle of the tree, find the new root
    //TODO maybe add a deterministic API instead of O(N) lookup
    if (prevElem && m_IsHead) {
        m_IsHead = false;

        auto root = prevElem;
        while (auto prev = root->up())
            root = static_cast<QuickTreeViewItem*>(prev);

        root->move();
        Q_ASSERT(root->m_IsHead);
    }

    // So other items can be GCed without always resetting to 0x0, note that it
    // might be a good idea to extend SimpleFlickable to support a virtual
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

bool QuickTreeViewItem::flush()
{
    return true;
}

bool QuickTreeViewItem::remove()
{
    if (item()) {
        item()->setParent(nullptr);
        item()->setParentItem(nullptr);
        item()->setVisible(false);
    }

    auto nextElem = static_cast<QuickTreeViewItem*>(down());
    auto prevElem = static_cast<QuickTreeViewItem*>(up());

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

void QuickTreeViewItem::setSelected(bool s)
{
    context()->setContextProperty("isCurrentItem", s);
}
