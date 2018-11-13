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
#include "geometrycache_p.h"

class IndexMetadataPrivate
{
public:

    GeometryCache          *m_pGeometryTracker { new GeometryCache() };
    VisualTreeItem         *m_pViewTracker     {       nullptr       };
    TreeTraversalItem      *m_pModelTracker    {       nullptr       };
    ViewItemContextAdapter *m_pContextAdapter  {       nullptr       };
    Viewport               *m_pViewport        {       nullptr       };

    typedef bool(IndexMetadataPrivate::*StateF)();
    static const IndexMetadataPrivate::StateF m_fStateMachine[5][7];

    bool nothing() {return true;}
    bool query  () {q_ptr->sizeHint(); return false;};

    IndexMetadata *q_ptr;
};

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

#define A &IndexMetadataPrivate::
const IndexMetadataPrivate::StateF IndexMetadataPrivate::m_fStateMachine[5][7] = {
/*                 MOVE      RESIZE      PLACE     RESET      MODIFY    DECORATE     VIEW  */
/*INIT     */ { A nothing, A nothing, A nothing, A nothing, A query  , A nothing, A nothing},
/*SIZE     */ { A nothing, A nothing, A nothing, A nothing, A nothing, A nothing, A nothing},
/*POSITION */ { A nothing, A nothing, A nothing, A nothing, A nothing, A nothing, A nothing},
/*PENDING  */ { A nothing, A nothing, A nothing, A nothing, A nothing, A nothing, A nothing},
/*VALID    */ { A nothing, A nothing, A nothing, A nothing, A nothing, A nothing, A nothing},
};
#undef A

IndexMetadata::IndexMetadata(TreeTraversalItem *tti, Viewport *p) :
    d_ptr(new IndexMetadataPrivate())
{
    d_ptr->q_ptr           = this;
    d_ptr->m_pModelTracker = tti;
    d_ptr->m_pViewport     = p;
}

IndexMetadata::~IndexMetadata()
{
    if (d_ptr->m_pContextAdapter) {
        if (d_ptr->m_pContextAdapter->isActive())
            d_ptr->m_pContextAdapter->context()->setContextObject(nullptr);
        delete d_ptr->m_pContextAdapter;
    }

    delete d_ptr;
}

void IndexMetadata::setViewTracker(VisualTreeItem *i)
{
    if (d_ptr->m_pViewTracker && !i) {
        auto old = d_ptr->m_pViewTracker;
        d_ptr->m_pViewTracker = nullptr;
        d_ptr->m_pViewport->s_ptr->notifyRemoval(old->m_pGeometry);
    }

    if ((d_ptr->m_pViewTracker = i)) {
        i->m_pGeometry = this;

        // Assign the context object
        contextAdapter()->context();
    }
}

VisualTreeItem *IndexMetadata::viewTracker() const
{
    return d_ptr->m_pViewTracker;
}

TreeTraversalItem *IndexMetadata::modelTracker() const
{
    return d_ptr->m_pModelTracker;
}

QRectF IndexMetadata::decoratedGeometry() const
{
    switch(d_ptr->m_pGeometryTracker->state()) {
        case GeometryCache::State::VALID:
            break;
        case GeometryCache::State::INIT:
            Q_ASSERT(false);
            break;
        case GeometryCache::State::SIZE:
        case GeometryCache::State::POSITION:
            const_cast<IndexMetadata*>(this)->sizeHint();
    }

    const auto ret = d_ptr->m_pGeometryTracker->decoratedGeometry();

    Q_ASSERT(d_ptr->m_pGeometryTracker->state() == GeometryCache::State::VALID);

    return ret;
}

ContextAdapter* IndexMetadata::contextAdapter() const
{
    if (!d_ptr->m_pContextAdapter) {
        auto cm = d_ptr->m_pViewport->modelAdapter()->contextAdapterFactory();
        d_ptr->m_pContextAdapter = cm->createAdapter<ViewItemContextAdapter>(
            d_ptr->m_pViewport->modelAdapter()->view()->rootContext()
        );
        d_ptr->m_pContextAdapter->m_pGeometry = const_cast<IndexMetadata*>(this);

        // This will init the context now.
        contextAdapter()->context();
    }

    return d_ptr->m_pContextAdapter;
}

QModelIndex ViewItemContextAdapter::index() const
{
    return m_pGeometry->index();
}

AbstractItemAdapter* ViewItemContextAdapter::item() const
{
    return m_pGeometry->viewTracker() ? m_pGeometry->viewTracker()->d_ptr : nullptr;
}

bool IndexMetadata::isValid() const
{
    return d_ptr->m_pGeometryTracker->state() == GeometryCache::State::VALID ||
        d_ptr->m_pGeometryTracker->state() == GeometryCache::State::PENDING;
}

QSizeF IndexMetadata::sizeHint()
{
    QSizeF ret;

    auto s = d_ptr->m_pGeometryTracker->state();

    //TODO switch to function table
    if (s == GeometryCache::State::POSITION || s == GeometryCache::State::INIT) {
        switch (d_ptr->m_pViewport->sizeHintStrategy()) {
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
                Q_ASSERT(d_ptr->m_pViewport->modelAdapter()->hasSizeHints());

                ret = qobject_cast<SizeHintProxyModel*>(
                    d_ptr->m_pViewport->modelAdapter()->rawModel()
                )->sizeHintForIndex(index());

                static int i = 0;

                break;
            case Viewport::SizeHintStrategy::ROLE:
            case Viewport::SizeHintStrategy::DELEGATE:
                Q_ASSERT(false);
                break;
        }

        qDebug() << "\n\n\n\nSET SIZE" << ret << (int) d_ptr->m_pViewport->sizeHintStrategy();
        d_ptr->m_pGeometryTracker->setSize(ret);
    }

    s = d_ptr->m_pGeometryTracker->state();
    Q_ASSERT(s != GeometryCache::State::INIT);
    Q_ASSERT(s != GeometryCache::State::POSITION);

    if (s == GeometryCache::State::SIZE) {
        if (auto prev = up()) {

            // A word of warning, this is recursive
            const auto prevGeo = prev->decoratedGeometry();
            Q_ASSERT(prevGeo.y() != -1);
            d_ptr->m_pGeometryTracker->setPosition(QPointF(0.0, prevGeo.y() + prevGeo.height()));
        }
        else if (isTopItem()) {
            d_ptr->m_pGeometryTracker->setPosition(QPointF(0.0, 0.0));
            Q_ASSERT(d_ptr->m_pGeometryTracker->state() == GeometryCache::State::PENDING);
        }
    }

    Q_ASSERT(isValid());

    return ret;
}

bool IndexMetadata::performAction(GeometryAction a)
{
    const auto s = (int)d_ptr->m_pGeometryTracker->state();

    if ((d_ptr->*IndexMetadataPrivate::m_fStateMachine[s][(int)a])()) {
        d_ptr->m_pGeometryTracker->performAction(a);
        return true;
    }

    return false;
}

qreal IndexMetadata::borderDecoration(Qt::Edge e) const
{
    return d_ptr->m_pGeometryTracker->borderDecoration(e);
}

void IndexMetadata::setBorderDecoration(Qt::Edge e, qreal r)
{
    d_ptr->m_pGeometryTracker->setBorderDecoration(e, r);
}

void IndexMetadata::setSize(const QSizeF& s)
{
    d_ptr->m_pGeometryTracker->setSize(s);
}

void IndexMetadata::setPosition(const QPointF& p)
{
    d_ptr->m_pGeometryTracker->setPosition(p);
}

int IndexMetadata::removeMe() const
{
    return (int)d_ptr->m_pGeometryTracker->state();
}

bool IndexMetadata::isInSync() const
{
    if (d_ptr->m_pGeometryTracker->state() != GeometryCache::State::VALID) {
        qDebug() << "INVALID";
        return false;
    }

    const auto item = viewTracker()->item();

    // If it got that far, then it's probably not going to load, trying to fix
    // that should be done elsewhere. Lets just assume a null delegate.
    if (!item) {
        qDebug() << "NO ITEM";
        return true;
    }

    const auto geo = d_ptr->m_pGeometryTracker->contentGeometry();

    // The actual QQuickItem position adjusted for the decoration
    QRectF correctedRect(
        item->x(),
        item->y(),
        item->width(),
        item->height()
    );

    //qDebug() << m_State.geometry();
    qDebug() << geo << d_ptr->m_pGeometryTracker->borderDecoration(Qt::TopEdge);
    qDebug() << correctedRect;

    return correctedRect == geo;
}
