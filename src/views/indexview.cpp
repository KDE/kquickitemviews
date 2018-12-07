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
#include "indexview.h"

// Qt
#include <QQmlEngine>
#include <QQmlContext>

// KQuickItemViews
#include <contextadapterfactory.h>
#include <adapters/contextadapter.h>

class IndexViewPrivate : public QObject
{
    Q_OBJECT
public:
    QQmlComponent         *m_pComponent {nullptr};
    QAbstractItemModel    *m_pModel     {nullptr};
    ContextAdapter        *m_pCTX       {nullptr};
    ContextAdapterFactory *m_pFactory   {nullptr};
    QQuickItem            *m_pItem      {nullptr};
    QPersistentModelIndex  m_Index      {       };

    // Helper
    void initDelegate();

    IndexView *q_ptr;

public Q_SLOTS:
    void slotDismiss();
    void slotDataChanged(const QModelIndex &tl, const QModelIndex &br, const QVector<int> &roles);
    void slotRemoved(const QModelIndex &parent, int first, int last);
};

IndexView::IndexView(QQuickItem *parent) : QQuickItem(parent),
    d_ptr(new IndexViewPrivate())
{
    d_ptr->q_ptr = this;
}

IndexView::~IndexView()
{
    if (d_ptr->m_pItem)
        delete d_ptr->m_pItem;

    if (d_ptr->m_pCTX)
        delete d_ptr->m_pCTX;

    if (d_ptr->m_pFactory)
        delete d_ptr->m_pFactory;

    delete d_ptr;
}

void IndexView::setDelegate(QQmlComponent* delegate)
{
    if (delegate == d_ptr->m_pComponent)
        return;

    if (d_ptr->m_pItem)
        delete d_ptr->m_pItem;

    d_ptr->m_pComponent = delegate;
    emit delegateChanged(delegate);

    d_ptr->initDelegate();
}

QQmlComponent* IndexView::delegate() const
{
    return d_ptr->m_pComponent;
}

QModelIndex IndexView::modelIndex() const
{
    return d_ptr->m_Index;
}

void IndexView::setModelIndex(const QModelIndex &index)
{
    if (index == d_ptr->m_Index)
        return;

    // Disconnect old models
    if (d_ptr->m_pModel && d_ptr->m_pModel != index.model())
        d_ptr->slotDismiss();


    if (d_ptr->m_pModel != index.model()) {

        d_ptr->m_pModel = const_cast<QAbstractItemModel*>(index.model());

        if (!d_ptr->m_pFactory)
            d_ptr->m_pFactory = new ContextAdapterFactory();

        d_ptr->m_pFactory->setModel(d_ptr->m_pModel);

        const auto ctx = QQmlEngine::contextForObject(this);

        if (d_ptr->m_pItem) {
            delete d_ptr->m_pItem;
            d_ptr->m_pItem = nullptr;
        }

        d_ptr->m_pCTX = d_ptr->m_pFactory->createAdapter(ctx);
        Q_ASSERT(d_ptr->m_pCTX->context()->parentContext() == ctx);

        connect(d_ptr->m_pModel, &QAbstractItemModel::destroyed,
            d_ptr, &IndexViewPrivate::slotDismiss);
        connect(d_ptr->m_pModel, &QAbstractItemModel::dataChanged,
            d_ptr, &IndexViewPrivate::slotDataChanged);
        connect(d_ptr->m_pModel, &QAbstractItemModel::rowsAboutToBeRemoved,
            d_ptr, &IndexViewPrivate::slotRemoved);
    }

    d_ptr->m_Index = index;
    d_ptr->m_pCTX->setModelIndex(index);

    d_ptr->initDelegate();
}

void IndexViewPrivate::slotDismiss()
{
    if (m_pItem) {
        delete m_pItem;
        m_pItem = nullptr;
    }

    disconnect(m_pModel, &QAbstractItemModel::destroyed,
        this, &IndexViewPrivate::slotDismiss);
    disconnect(m_pModel, &QAbstractItemModel::dataChanged,
        this, &IndexViewPrivate::slotDataChanged);
    disconnect(m_pModel, &QAbstractItemModel::rowsAboutToBeRemoved,
        this, &IndexViewPrivate::slotRemoved);

    if (m_pCTX) {
        delete m_pCTX;
        m_pCTX = nullptr;
    }

    m_pModel = nullptr;
    m_Index  = QModelIndex();
}

void IndexViewPrivate::slotDataChanged(const QModelIndex &tl, const QModelIndex &br, const QVector<int> &roles)
{
    if (tl.parent() != m_Index.parent())
        return;

    if (tl.row() > m_Index.row() || br.row() < m_Index.row())
        return;

    if (tl.column() > m_Index.column() || br.column() < m_Index.column())
        return;

    m_pCTX->updateRoles(roles);
}

void IndexViewPrivate::slotRemoved(const QModelIndex &parent, int first, int last)
{
    if (parent != m_Index.parent() || !(m_Index.row() >= first && m_Index.row() <= last))
        return;

    slotDismiss();
}

void IndexViewPrivate::initDelegate()
{
    if (m_pItem || (!m_pComponent) || !m_Index.isValid())
        return;

    m_pItem = qobject_cast<QQuickItem *>(m_pComponent->create(m_pCTX->context()));

    // It will happen when the QML itself is invalid
    if (!m_pItem)
        return;

    const auto ctx = QQmlEngine::contextForObject(q_ptr);
    Q_ASSERT(ctx);

    ctx->engine()->setObjectOwnership(m_pItem, QQmlEngine::CppOwnership);
    m_pItem->setParentItem(q_ptr);
    volatile auto cc = QQmlEngine::contextForObject(m_pItem);
}

#include <indexview.moc>
