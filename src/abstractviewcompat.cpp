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
#include "abstractviewcompat.h"

#include <abstractselectableview.h>
#include "abstractquickview_p.h"
#include "contextmanager.h"
#include "modeladapter.h"
#include "visiblerange.h"

class AbstractViewCompatPrivate
{
public:
    Qt::Corner m_Corner {Qt::TopLeftCorner};
    bool m_IsSortingEnabled{ false };
    ModelAdapter *m_pModelAdapter;

    AbstractViewCompat* q_ptr;
};

AbstractViewCompat::AbstractViewCompat(QQuickItem* parent) : AbstractQuickView(parent),
    d_ptr(new AbstractViewCompatPrivate())
{
    d_ptr->q_ptr = this;

    d_ptr->m_pModelAdapter = new ModelAdapter(this);
    addModelAdapter(d_ptr->m_pModelAdapter);

    auto sm = d_ptr->m_pModelAdapter->selectionManager();

    // Ok, connecting signals to signals is not a very good idea, I am lazy
    connect(sm, &AbstractSelectableView::currentIndexChanged,
        this, &AbstractViewCompat::currentIndexChanged);
    connect(sm, &AbstractSelectableView::selectionModelChanged,
        this, &AbstractViewCompat::selectionModelChanged);
    connect(d_ptr->m_pModelAdapter, &ModelAdapter::countChanged,
        this, &AbstractViewCompat::countChanged);
    connect(d_ptr->m_pModelAdapter, &ModelAdapter::modelAboutToChange,
        this, &AbstractViewCompat::applyModelChanges);
}

AbstractViewCompat::~AbstractViewCompat()
{
    delete d_ptr;
}


Qt::Corner AbstractViewCompat::gravity() const
{
    return d_ptr->m_Corner;
}

void AbstractViewCompat::setGravity(Qt::Corner g)
{
    d_ptr->m_Corner = g;
    refresh();
}

QQmlComponent* AbstractViewCompat::highlight() const
{
    return d_ptr->m_pModelAdapter->selectionManager()->highlight();
}

void AbstractViewCompat::setHighlight(QQmlComponent* h)
{
    d_ptr->m_pModelAdapter->selectionManager()->setHighlight(h);
}

void AbstractViewCompat::setDelegate(QQmlComponent* delegate)
{
    d_ptr->m_pModelAdapter->setDelegate(delegate);
    emit delegateChanged(delegate);
    refresh();
}

QQmlComponent* AbstractViewCompat::delegate() const
{
    return d_ptr->m_pModelAdapter->delegate();
}

QSharedPointer<QItemSelectionModel> AbstractViewCompat::selectionModel() const
{
    return d_ptr->m_pModelAdapter->selectionManager()->selectionModel();
}

QVariant AbstractViewCompat::model() const
{
    return d_ptr->m_pModelAdapter->model();
}

void AbstractViewCompat::setModel(const QVariant& m)
{
    d_ptr->m_pModelAdapter->setModel(m);
}

void AbstractViewCompat::setSelectionModel(QSharedPointer<QItemSelectionModel> m)
{
    d_ptr->m_pModelAdapter->selectionManager()->setSelectionModel(m);
}

QAbstractItemModel *AbstractViewCompat::rawModel() const
{
    return d_ptr->m_pModelAdapter->rawModel();
}

void AbstractViewCompat::applyModelChanges(QAbstractItemModel* m)
{
    if (d_ptr->m_IsSortingEnabled && m) {
        m->sort(0);
    }
}

bool AbstractViewCompat::isSortingEnabled() const
{
    return d_ptr->m_IsSortingEnabled;
}

void AbstractViewCompat::setSortingEnabled(bool val)
{
    d_ptr->m_IsSortingEnabled = val;

    if (d_ptr->m_IsSortingEnabled && rawModel()) {
        rawModel()->sort(0);
    }
}

QModelIndex AbstractViewCompat::currentIndex() const
{
    return selectionModel()->currentIndex();
}

void AbstractViewCompat::setCurrentIndex(const QModelIndex& index, QItemSelectionModel::SelectionFlags f)
{
    selectionModel()->setCurrentIndex(index, f);
}

bool AbstractViewCompat::hasUniformRowHeight() const
{
    return d_ptr->m_pModelAdapter->visibleRanges().constFirst()->
        sizeHintStrategy() == VisibleRange::SizeHintStrategy::UNIFORM;
}

void AbstractViewCompat::setUniformRowHeight(bool value)
{
    //d_ptr->m_pModelAdapter->setUniformRowHeight(value);
}

bool AbstractViewCompat::hasUniformColumnWidth() const
{
    return d_ptr->m_pModelAdapter->visibleRanges().constFirst()->
        sizeHintStrategy() == VisibleRange::SizeHintStrategy::UNIFORM;
}

void AbstractViewCompat::setUniformColumnColumnWidth(bool value)
{
    //d_ptr->m_pModelAdapter->setUniformColumnColumnWidth(value);
}

bool AbstractViewCompat::isEmpty() const
{
    return d_ptr->m_pModelAdapter->isEmpty();
}
