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
#include "singlemodelviewbase.h"

// Qt
#include <QtCore/QTimer>

// KQuickItemViews
#include <KQuickItemViews/adapters/selectionadapter.h>
#include <KQuickItemViews/adapters/contextadapter.h>
#include <KQuickItemViews/adapters/modeladapter.h>
#include "viewport.h"
#include "private/viewport_p.h"
#include "private/geostrategyselector_p.h"

class SingleModelViewBasePrivate
{
public:
    bool          m_IsSortingEnabled {  false  };
    ModelAdapter *m_pModelAdapter    { nullptr };

    SingleModelViewBase* q_ptr;
};

SingleModelViewBase::SingleModelViewBase(ItemFactoryBase *factory, QQuickItem* parent) : ViewBase(parent),
    d_ptr(new SingleModelViewBasePrivate())
{
    d_ptr->q_ptr = this;

    d_ptr->m_pModelAdapter = new ModelAdapter(this);
    addModelAdapter(d_ptr->m_pModelAdapter);

    auto vp = d_ptr->m_pModelAdapter->viewports().first();
    vp->setItemFactory(factory);

    auto sm = d_ptr->m_pModelAdapter->selectionAdapter();

    // Ok, connecting signals to signals is not a very good idea, I am lazy
    connect(sm, &SelectionAdapter::currentIndexChanged,
        this, &SingleModelViewBase::currentIndexChanged);
    connect(sm, &SelectionAdapter::selectionModelChanged,
        this, &SingleModelViewBase::selectionModelChanged);
    connect(d_ptr->m_pModelAdapter, &ModelAdapter::modelAboutToChange,
        this, &SingleModelViewBase::applyModelChanges);
    connect(vp, &Viewport::cornerChanged,
        this, &SingleModelViewBase::cornerChanged);
}

SingleModelViewBase::~SingleModelViewBase()
{
    delete d_ptr;
}

QQmlComponent* SingleModelViewBase::highlight() const
{
    return d_ptr->m_pModelAdapter->selectionAdapter()->highlight();
}

void SingleModelViewBase::setHighlight(QQmlComponent* h)
{
    d_ptr->m_pModelAdapter->selectionAdapter()->setHighlight(h);
}

void SingleModelViewBase::setDelegate(QQmlComponent* delegate)
{
    d_ptr->m_pModelAdapter->setDelegate(delegate);
    emit delegateChanged(delegate);
    refresh();
}

QQmlComponent* SingleModelViewBase::delegate() const
{
    return d_ptr->m_pModelAdapter->delegate();
}

QSharedPointer<QItemSelectionModel> SingleModelViewBase::selectionModel() const
{
    return d_ptr->m_pModelAdapter->selectionAdapter()->selectionModel();
}

QVariant SingleModelViewBase::model() const
{
    return d_ptr->m_pModelAdapter->model();
}

void SingleModelViewBase::setModel(const QVariant& m)
{
    d_ptr->m_pModelAdapter->setModel(m);
    emit modelChanged();
}

void SingleModelViewBase::setSelectionModel(QSharedPointer<QItemSelectionModel> m)
{
    d_ptr->m_pModelAdapter->selectionAdapter()->setSelectionModel(m);
    emit selectionModelChanged();
}

QAbstractItemModel *SingleModelViewBase::rawModel() const
{
    return d_ptr->m_pModelAdapter->rawModel();
}

void SingleModelViewBase::applyModelChanges(QAbstractItemModel* m)
{
    if (d_ptr->m_IsSortingEnabled && m) {
        m->sort(0);
    }
}

bool SingleModelViewBase::isDelegateSizeForced() const
{
    return d_ptr->m_pModelAdapter->viewports().constFirst()->s_ptr->
        m_pGeoAdapter->isSizeForced();
}

void SingleModelViewBase::setDelegateSizeForced(bool f)
{
    d_ptr->m_pModelAdapter->viewports().constFirst()->s_ptr->
        m_pGeoAdapter->setSizeForced(f);
}

bool SingleModelViewBase::isSortingEnabled() const
{
    return d_ptr->m_IsSortingEnabled;
}

void SingleModelViewBase::setSortingEnabled(bool val)
{
    d_ptr->m_IsSortingEnabled = val;

    if (d_ptr->m_IsSortingEnabled && rawModel()) {
        rawModel()->sort(0);
    }
}

QModelIndex SingleModelViewBase::currentIndex() const
{
    return selectionModel()->currentIndex();
}

void SingleModelViewBase::setCurrentIndex(const QModelIndex& index, QItemSelectionModel::SelectionFlags f)
{
    selectionModel()->setCurrentIndex(index, f);
}

bool SingleModelViewBase::hasUniformRowHeight() const
{
    return d_ptr->m_pModelAdapter->viewports().constFirst()->s_ptr->
        m_pGeoAdapter->capabilities() & GeometryAdapter::Capabilities::HAS_UNIFORM_HEIGHT;
}

void SingleModelViewBase::setUniformRowHeight(bool value)
{
    Q_UNUSED(value)
    //d_ptr->m_pModelAdapter->setUniformRowHeight(value);
}

bool SingleModelViewBase::hasUniformColumnWidth() const
{
    return d_ptr->m_pModelAdapter->viewports().constFirst()->s_ptr->
        m_pGeoAdapter->capabilities() & GeometryAdapter::Capabilities::HAS_UNIFORM_WIDTH;
}

void SingleModelViewBase::setUniformColumnColumnWidth(bool value)
{
    Q_UNUSED(value)
    //d_ptr->m_pModelAdapter->setUniformColumnColumnWidth(value);
}

void SingleModelViewBase::moveTo(Qt::Edge e)
{
    QTimer::singleShot(0, [this, e]() {
        //HACK This need the viewportAdapter to be optimized
        switch(e) {
            case Qt::TopEdge:
                setContentY(0);
                break;
            case Qt::BottomEdge: {
                int y = contentY();
                // Keep loading until it doesn't load anything else
                do {
                    setContentY(999999);
                } while (contentY() > y && (y = contentY()));
            }
            case Qt::LeftEdge:
            case Qt::RightEdge:
                break; //TODO
        }
    });
}

QModelIndex SingleModelViewBase::indexAt(const QPoint & point) const
{
    return d_ptr->m_pModelAdapter->viewports().first()->indexAt(point);
}

QModelIndex SingleModelViewBase::topLeft() const
{
    return d_ptr->m_pModelAdapter->viewports().first()->indexAt(Qt::TopLeftCorner);
}

QModelIndex SingleModelViewBase::topRight() const
{
    return d_ptr->m_pModelAdapter->viewports().first()->indexAt(Qt::TopRightCorner);
}

QModelIndex SingleModelViewBase::bottomLeft() const
{
    return d_ptr->m_pModelAdapter->viewports().first()->indexAt(Qt::BottomLeftCorner);
}

QModelIndex SingleModelViewBase::bottomRight() const
{
    return d_ptr->m_pModelAdapter->viewports().first()->indexAt(Qt::BottomRightCorner);
}


QRectF SingleModelViewBase::itemRect(const QModelIndex& i) const
{
    return d_ptr->m_pModelAdapter->viewports().first()->itemRect(i);
}
