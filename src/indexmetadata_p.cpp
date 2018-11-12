/***************************************************************************
 *   Copyright (C) 2018 by Emmanuel Lepage Vallee                          *
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
#include "indexmetadata_p.h"

// Qt
#include <QQmlContext>

//
#include "adapters/abstractitemadapter_p.h"
#include "adapters/contextadapter.h"
#include "adapters/modeladapter.h"
#include "viewport.h"
#include "viewport_p.h"
#include "contextadapterfactory.h"
#include "proxies/sizehintproxymodel.h"

/**
 * Extend the class for the need of needs of the AbstractItemAdapter.
 */
class ViewItemContextAdapter final : public ContextAdapter
{
public:
    explicit ViewItemContextAdapter(QQmlContext* p) : ContextAdapter(p){}
    virtual ~ViewItemContextAdapter() {
        flushCache();
    }

    virtual QModelIndex          index  () const override;
    virtual AbstractItemAdapter *item   () const override;

    IndexMetadata* m_pGeometry;
};

IndexMetadata::IndexMetadata(TreeTraversalItem *tti, Viewport *p) :
    m_pTTI(tti), m_pViewport(p)
{
}

IndexMetadata::~IndexMetadata()
{
    if (m_pContextAdapter) {
        if (m_pContextAdapter->isActive())
            m_pContextAdapter->context()->setContextObject(nullptr);
        delete m_pContextAdapter;
    }
}

void IndexMetadata::setViewTracker(VisualTreeItem *i)
{
    if (m_pItem && !i) {
        auto old = m_pItem;
        m_pItem = nullptr;
        m_pViewport->s_ptr->notifyRemoval(old->m_pGeometry);
    }

    if ((m_pItem = i)) {
        i->m_pGeometry = this;

        // Assign the context object
        contextAdapter()->context();
    }
}

VisualTreeItem *IndexMetadata::viewTracker() const
{
    return m_pItem;
}

TreeTraversalItem *IndexMetadata::modelTracker() const
{
    return m_pTTI;
}

QRectF IndexMetadata::geometry() const
{
    switch(m_State.state()) {
        case GeometryCache::State::VALID:
            break;
        case GeometryCache::State::INIT:
            Q_ASSERT(false);
            break;
        case GeometryCache::State::SIZE:
        case GeometryCache::State::POSITION:
            const_cast<IndexMetadata*>(this)->sizeHint();
    }

    const auto ret = m_State.decoratedGeometry();

    Q_ASSERT(m_State.state() == GeometryCache::State::VALID);

    return ret;
}

ContextAdapter* IndexMetadata::contextAdapter() const
{
    if (!m_pContextAdapter) {
        auto cm = m_pViewport->modelAdapter()->contextAdapterFactory();
        m_pContextAdapter = cm->createAdapter<ViewItemContextAdapter>(
            m_pViewport->modelAdapter()->view()->rootContext()
        );
        m_pContextAdapter->m_pGeometry = const_cast<IndexMetadata*>(this);

        // This will init the context now.
        contextAdapter()->context();
    }

    return m_pContextAdapter;
}

QModelIndex ViewItemContextAdapter::index() const
{
    return m_pGeometry->index();
}

AbstractItemAdapter* ViewItemContextAdapter::item() const
{
    return m_pGeometry->viewTracker() ? m_pGeometry->viewTracker()->d_ptr : nullptr;
}

QSizeF IndexMetadata::sizeHint()
{
    QSizeF ret;

    auto s = m_State.state();

    //TODO switch to function table
    if (s == GeometryCache::State::POSITION || s == GeometryCache::State::INIT) {
        switch (m_pViewport->sizeHintStrategy()) {
            case Viewport::SizeHintStrategy::AOT:
                Q_ASSERT(false);
                break;
            case Viewport::SizeHintStrategy::JIT:
                if (viewTracker())
                    ret = viewTracker()->geometry().size();
                else {
                    // JIT cannot be used past the loaded bounds, the value isn't known
                    Q_ASSERT(false);
                }
                break;
            case Viewport::SizeHintStrategy::UNIFORM:
                Q_ASSERT(false);
                break;
            case Viewport::SizeHintStrategy::PROXY:
                Q_ASSERT(m_pViewport->modelAdapter()->hasSizeHints());

                ret = qobject_cast<SizeHintProxyModel*>(
                    m_pViewport->modelAdapter()->rawModel()
                )->sizeHintForIndex(index());

                static int i = 0;

                break;
            case Viewport::SizeHintStrategy::ROLE:
            case Viewport::SizeHintStrategy::DELEGATE:
                Q_ASSERT(false);
                break;
        }

        qDebug() << "\n\n\n\nSET SIZE" << ret << (int) m_pViewport->sizeHintStrategy();
        m_State.setSize(ret);
    }

    s = m_State.state();
    Q_ASSERT(s != GeometryCache::State::INIT);
    Q_ASSERT(s != GeometryCache::State::POSITION);

    if (s == GeometryCache::State::SIZE) {
        if (auto prev = up()) {

            // A word of warning, this is recursive
            const auto prevGeo = prev->geometry();
            Q_ASSERT(prevGeo.y() != -1);
            m_State.setPosition(QPointF(0.0, prevGeo.y() + prevGeo.height()));
        }
        else if (isTopItem()) {
            m_State.setPosition(QPointF(0.0, 0.0));
            Q_ASSERT(m_State.state() == GeometryCache::State::PENDING);
        }
    }

    Q_ASSERT(m_State.isReady());

    return ret;
}
