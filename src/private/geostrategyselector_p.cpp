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
#include "geostrategyselector_p.h"

// Qt
#include <QtCore/QAbstractItemModel>
#include <QtCore/QDebug>

// KQuickItemViews
#include <proxies/sizehintproxymodel.h>
#include <strategies/justintime.h>
#include <strategies/proxy.h>
#include <strategies/role.h>
#include <strategies/delegate.h>
#include <strategies/aheadoftime.h>
#include <strategies/uniform.h>


class GeoStrategySelectorPrivate : public QObject
{
public:
    explicit GeoStrategySelectorPrivate(GeoStrategySelector *q) : QObject(q), q_ptr(q) {}

    enum class BuiltInStrategies {
        AOT     , /*!< Load everything ahead of time, doesn't scale but very reliable */
        JIT     , /*!< Do not try to compute the total size, scrollbars wont work     */
        UNIFORM , /*!< Assume all elements have the same size, scales well when true  */
        PROXY   , /*!< Use a QSizeHintProxyModel, require work by all developers      */
        ROLE    , /*!< Use one of the QAbstractItemModel role as size                 */
        DELEGATE, /*!< Assume the view re-implemented ::sizeHint is correct           */
    };

    BuiltInStrategies m_CurrentStrategy { BuiltInStrategies::JIT };

    GeometryAdapter    *m_A      {nullptr};
    QAbstractItemModel *m_pModel {nullptr};

    uint m_Features {GeoStrategySelector::Features::NONE};

    bool checkHasContent();
    bool checkHasRole   ();
    bool checkProxyModel();

    void optimize();

    void replaceStrategy(BuiltInStrategies s);

    GeoStrategySelector *q_ptr;

public Q_SLOTS:
    void slotRowsInserted();
};

GeoStrategySelector::GeoStrategySelector(Viewport *parent) : GeometryAdapter(parent),
    d_ptr(new GeoStrategySelectorPrivate(this))
{
    d_ptr->m_A = new GeometryStrategies::JustInTime(parent);
}

GeoStrategySelector::~GeoStrategySelector()
{}

QSizeF GeoStrategySelector::sizeHint(const QModelIndex& i, AbstractItemAdapter *a) const
{
    return d_ptr->m_A ?
        d_ptr->m_A->sizeHint(i, a) : GeometryAdapter::sizeHint(i, a);
}

QPointF GeoStrategySelector::positionHint(const QModelIndex& i, AbstractItemAdapter *a) const
{
    return d_ptr->m_A ?
        d_ptr->m_A->positionHint(i, a) : GeometryAdapter::positionHint(i, a);
}

int GeoStrategySelector::capabilities() const
{
    return d_ptr->m_A ?
        d_ptr->m_A->capabilities() : GeometryAdapter::capabilities();
}

void GeoStrategySelector::setModel(QAbstractItemModel *m)
{
    if (m == d_ptr->m_pModel)
        return;

    d_ptr->m_pModel = m;

    static constexpr auto toClean = Features::HAS_MODEL
        | Features::HAS_SHP_MODEL
        | Features::HAS_MODEL_CONTENT
        | Features::HAS_SIZE_ROLE;

    d_ptr->m_Features = d_ptr->m_Features & (~toClean);

    // Reload the model-dependent features
    d_ptr->m_Features |= m ?
        Features::HAS_MODEL : Features::NONE;
    d_ptr->m_Features |= d_ptr->checkHasContent() ?
        Features::HAS_MODEL_CONTENT : Features::NONE;
    d_ptr->m_Features |= d_ptr->checkHasRole() ?
        Features::HAS_SIZE_ROLE : Features::NONE;
    d_ptr->m_Features |= d_ptr->checkProxyModel() ?
        Features::HAS_SHP_MODEL : Features::NONE;

    d_ptr->optimize();
}

void GeoStrategySelector::setHasScrollbar(bool v)
{
    d_ptr->m_Features = d_ptr->m_Features & (~Features::HAS_SCROLLBAR);
    d_ptr->m_Features |= v ? Features::HAS_SCROLLBAR : Features::NONE;
}

void GeoStrategySelectorPrivate::slotRowsInserted()
{
    checkHasRole();

    // Now that the introspection is done, there is no further need for this
    disconnect(m_pModel, &QAbstractItemModel::rowsInserted,
        this, &GeoStrategySelectorPrivate::slotRowsInserted);
}

bool GeoStrategySelectorPrivate::checkHasContent()
{
    return m_pModel->index(0, 0).isValid();
}

bool GeoStrategySelectorPrivate::checkHasRole()
{
    const QModelIndex i = m_pModel->index(0, 0);

    return i.data(Qt::SizeHintRole).isValid();
}

bool GeoStrategySelectorPrivate::checkProxyModel()
{
    return m_pModel && m_pModel->metaObject()->inherits(
        &SizeHintProxyModel::staticMetaObject
    );
}

void GeoStrategySelectorPrivate::optimize()
{
    // Here will eventually reside the main optimization algorithm. For now
    // just choose between the JustInTime and Proxy adapters, the only ones
    // fully implemented.

    BuiltInStrategies next = m_CurrentStrategy;

    if (m_Features & GeoStrategySelector::Features::HAS_SHP_MODEL) {
        next = BuiltInStrategies::PROXY;
    }
    else {
        next = BuiltInStrategies::JIT;
    }

    if (next != m_CurrentStrategy) {
        replaceStrategy(next);
    }
}

void GeoStrategySelectorPrivate::replaceStrategy(BuiltInStrategies s)
{
    delete m_A;
    m_A = nullptr;

    switch(s) {
        case BuiltInStrategies::AOT:
            //TODO this requires ViewportAdapter to work
            break;
        case BuiltInStrategies::JIT:
            m_A = new GeometryStrategies::JustInTime(q_ptr->viewport());
            break;
        case BuiltInStrategies::UNIFORM:
            m_A = new GeometryStrategies::Uniform(q_ptr->viewport());
            break;
        case BuiltInStrategies::PROXY:
            m_A = new GeometryStrategies::Proxy(q_ptr->viewport());
            break;
        case BuiltInStrategies::ROLE:
            m_A = new GeometryStrategies::Role(q_ptr->viewport());
            break;
        case BuiltInStrategies::DELEGATE:
            m_A = new GeometryStrategies::Delegate(q_ptr->viewport());
            break;
    }

    emit q_ptr->dismissResult();
}
