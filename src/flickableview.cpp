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
#include "flickableview.h"

#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlPropertyMap>
#include <QtCore/QItemSelectionModel>

#include "sizehintproxymodel.h"

class FlickableViewPrivate : public QObject
{
    Q_OBJECT
public:
    QSharedPointer<QAbstractItemModel>  m_pModel          {nullptr};
//     QSharedPointer<QItemSelectionModel> m_pSelectionModel {nullptr};
    QQmlComponent*                      m_pDelegate       {nullptr};
    QQmlEngine*                         m_pEngine         {nullptr};
    QQmlComponent*                      m_pComponent      {nullptr};
    mutable QQmlContext*                m_pRootContext    {nullptr};
    QSharedPointer<QAbstractItemModel>  m_pNextModel      {nullptr};

    bool m_ModelHasSizeHints {false};
    QByteArray m_SizeHintRole;
    int m_SizeHintRoleIndex {-1};

    FlickableView* q_ptr;
};

FlickableView::FlickableView(QQuickItem* parent) : SimpleFlickable(parent),
    d_ptr(new FlickableViewPrivate())
{
    d_ptr->q_ptr = this;
}

FlickableView::~FlickableView()
{
    delete d_ptr;
}

void FlickableView::applyModelChanges(QAbstractItemModel* m)
{
    Q_ASSERT(m == d_ptr->m_pNextModel);

    d_ptr->m_pModel = d_ptr->m_pNextModel;

    emit modelChanged(d_ptr->m_pModel);
    emit countChanged();

    refresh();
    setCurrentY(contentHeight());

    d_ptr->m_pNextModel = nullptr;
}

void FlickableView::setModel(QSharedPointer<QAbstractItemModel> model)
{
    if (d_ptr->m_pNextModel == model || (d_ptr->m_pModel == model && !d_ptr->m_pNextModel))
        return;

    d_ptr->m_pNextModel = model;

    // Check if the proxyModel is used
    d_ptr->m_ModelHasSizeHints = model->metaObject()->inherits(
        &SizeHintProxyModel::staticMetaObject
    );

    if (!d_ptr->m_SizeHintRole.isEmpty() && model)
        d_ptr->m_SizeHintRoleIndex = model->roleNames().key(
            d_ptr->m_SizeHintRole
        );

    applyModelChanges(d_ptr->m_pNextModel.data());
}

void FlickableView::setRawModel(QAbstractItemModel* m)
{
    //FIXME blatant leak
    auto p = new QSharedPointer<QAbstractItemModel>(m);

    setModel(*p);
}

QSharedPointer<QAbstractItemModel> FlickableView::model() const
{
    return d_ptr->m_pModel;
}

void FlickableView::setDelegate(QQmlComponent* delegate)
{
    d_ptr->m_pDelegate = delegate;
    emit delegateChanged(delegate);
    refresh();
}

QQmlComponent* FlickableView::delegate() const
{
    return d_ptr->m_pDelegate;
    //refresh();
}

QQmlContext* FlickableView::rootContext() const
{
    if (!d_ptr->m_pRootContext)
        d_ptr->m_pRootContext = QQmlEngine::contextForObject(this);

    return d_ptr->m_pRootContext;
}

bool FlickableView::isEmpty() const
{
    return d_ptr->m_pModel ? !d_ptr->m_pModel->rowCount() : true;
}

QSizeF FlickableView::sizeHint(const QModelIndex& index) const
{
    if (!d_ptr->m_SizeHintRole.isEmpty())
        return index.data(d_ptr->m_SizeHintRoleIndex).toSize();

    if (!d_ptr->m_pEngine)
        d_ptr->m_pEngine = rootContext()->engine();

    if (d_ptr->m_ModelHasSizeHints)
        return qobject_cast<SizeHintProxyModel*>(model().data())
            ->sizeHintForIndex(d_ptr->m_pEngine, index);

    return {};
}

QString FlickableView::sizeHintRole() const
{
    return d_ptr->m_SizeHintRole;
}

void FlickableView::setSizeHintRole(const QString& s)
{
    d_ptr->m_SizeHintRole = s.toLatin1();

    if (!d_ptr->m_SizeHintRole.isEmpty() && model())
        d_ptr->m_SizeHintRoleIndex = model()->roleNames().key(
            d_ptr->m_SizeHintRole
        );
}

#include <flickableview.moc>
