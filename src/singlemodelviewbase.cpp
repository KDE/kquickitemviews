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

// KQuickItemViews
#include "adapters/selectionadapter.h"
#include "adapters/contextadapter.h"
#include "adapters/modeladapter.h"
#include "visiblerange.h"

class SingleModelViewBasePrivate
{
public:
    bool          m_IsSortingEnabled {      false      };
    ModelAdapter *m_pModelAdapter    {     nullptr     };

    SingleModelViewBase* q_ptr;
};

SingleModelViewBase::SingleModelViewBase(ItemFactoryBase *factory, QQuickItem* parent) : ViewBase(parent),
    d_ptr(new SingleModelViewBasePrivate())
{
    d_ptr->q_ptr = this;

    d_ptr->m_pModelAdapter = new ModelAdapter(this);
    addModelAdapter(d_ptr->m_pModelAdapter);

    d_ptr->m_pModelAdapter->visibleRanges().first()->setItemFactory(factory);

    auto sm = d_ptr->m_pModelAdapter->selectionAdapter();

    // Ok, connecting signals to signals is not a very good idea, I am lazy
    connect(sm, &SelectionAdapter::currentIndexChanged,
        this, &SingleModelViewBase::currentIndexChanged);
    connect(sm, &SelectionAdapter::selectionModelChanged,
        this, &SingleModelViewBase::selectionModelChanged);
//     connect(d_ptr->m_pModelAdapter, &ModelAdapter::countChanged,
//         this, &SingleModelViewBase::countChanged);
    connect(d_ptr->m_pModelAdapter, &ModelAdapter::modelAboutToChange,
        this, &SingleModelViewBase::applyModelChanges);
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
}

void SingleModelViewBase::setSelectionModel(QSharedPointer<QItemSelectionModel> m)
{
    d_ptr->m_pModelAdapter->selectionAdapter()->setSelectionModel(m);
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
    return d_ptr->m_pModelAdapter->visibleRanges().constFirst()->
        sizeHintStrategy() == VisibleRange::SizeHintStrategy::UNIFORM;
}

void SingleModelViewBase::setUniformRowHeight(bool value)
{
    Q_UNUSED(value)
    //d_ptr->m_pModelAdapter->setUniformRowHeight(value);
}

bool SingleModelViewBase::hasUniformColumnWidth() const
{
    return d_ptr->m_pModelAdapter->visibleRanges().constFirst()->
        sizeHintStrategy() == VisibleRange::SizeHintStrategy::UNIFORM;
}

void SingleModelViewBase::setUniformColumnColumnWidth(bool value)
{
    Q_UNUSED(value)
    //d_ptr->m_pModelAdapter->setUniformColumnColumnWidth(value);
}

bool SingleModelViewBase::isEmpty() const
{
    return d_ptr->m_pModelAdapter->isEmpty();
}
