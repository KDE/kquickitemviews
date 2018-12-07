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
#include <extensions/contextextension.h>
#include <qmodelindexwatcher.h>

/// Make QModelIndexBinder life easier
class MIWContextExtension final : public ContextExtension
{
public:
    virtual ~MIWContextExtension() {}
    virtual QVector<QByteArray>& propertyNames() const override;
    virtual QVariant getProperty(AbstractItemAdapter* item, uint id, const QModelIndex& index) const override;

    IndexViewPrivate *d_ptr;
};

class IndexViewPrivate : public QObject
{
    Q_OBJECT
public:
    QQmlComponent         *m_pComponent {nullptr};
    ContextAdapter        *m_pCTX       {nullptr};
    ContextAdapterFactory *m_pFactory   {nullptr};
    QQuickItem            *m_pItem      {nullptr};
    MIWContextExtension   *m_pExt       {nullptr};
    QModelIndexWatcher    *m_pWatcher   {nullptr};

    // Helper
    void initDelegate();

    IndexView *q_ptr;

public Q_SLOTS:
    void slotDismiss();
    void slotDataChanged(const QVector<int> &roles);
};

IndexView::IndexView(QQuickItem *parent) : QQuickItem(parent),
    d_ptr(new IndexViewPrivate())
{
    d_ptr->q_ptr = this;
    d_ptr->m_pWatcher = new QModelIndexWatcher(this);
    connect(d_ptr->m_pWatcher, &QModelIndexWatcher::removed,
        d_ptr, &IndexViewPrivate::slotDismiss);
    connect(d_ptr->m_pWatcher, &QModelIndexWatcher::dataChanged,
        d_ptr, &IndexViewPrivate::slotDataChanged);
}

IndexView::~IndexView()
{
    if (d_ptr->m_pItem)
        delete d_ptr->m_pItem;

    if (d_ptr->m_pCTX)
        delete d_ptr->m_pCTX;

    if (d_ptr->m_pFactory)
        delete d_ptr->m_pFactory;

    if (d_ptr->m_pExt)
        delete d_ptr->m_pExt;

    delete d_ptr->m_pWatcher;
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
    return d_ptr->m_pWatcher->modelIndex();
}

QAbstractItemModel *IndexView::model() const
{
    return d_ptr->m_pWatcher->model();
}

void IndexView::setModelIndex(const QModelIndex &index)
{
    if (index == modelIndex())
        return;

    // Disconnect old models
    if (model() && model() != index.model())
        d_ptr->slotDismiss();


    if (model() != index.model()) {
        if (!d_ptr->m_pFactory) {
            d_ptr->m_pExt        = new MIWContextExtension  ();
            d_ptr->m_pFactory    = new ContextAdapterFactory();
            d_ptr->m_pExt->d_ptr = d_ptr;
            d_ptr->m_pFactory->addContextExtension(d_ptr->m_pExt);
        }

        d_ptr->m_pFactory->setModel(const_cast<QAbstractItemModel*>(index.model()));

        const auto ctx = QQmlEngine::contextForObject(this);

        if (d_ptr->m_pItem) {
            delete d_ptr->m_pItem;
            d_ptr->m_pItem = nullptr;
        }

        d_ptr->m_pCTX = d_ptr->m_pFactory->createAdapter(ctx);
        Q_ASSERT(d_ptr->m_pCTX->context()->parentContext() == ctx);
    }

    d_ptr->m_pWatcher->setModelIndex(index);
    d_ptr->m_pCTX    ->setModelIndex(index);

    d_ptr->initDelegate();
    emit indexChanged();
}

void IndexViewPrivate::slotDismiss()
{
    if (m_pItem) {
        delete m_pItem;
        m_pItem = nullptr;
    }

    if (m_pCTX) {
        delete m_pCTX;
        m_pCTX = nullptr;
    }

    emit q_ptr->indexChanged();
}

void IndexViewPrivate::slotDataChanged(const QVector<int> &roles)
{
    m_pCTX->updateRoles(roles);
}

void IndexViewPrivate::initDelegate()
{
    if (m_pItem || (!m_pComponent) || !q_ptr->modelIndex().isValid())
        return;

    m_pItem = qobject_cast<QQuickItem *>(m_pComponent->create(m_pCTX->context()));

    // It will happen when the QML itself is invalid
    if (!m_pItem)
        return;

    const auto ctx = QQmlEngine::contextForObject(q_ptr);
    Q_ASSERT(ctx);

    ctx->engine()->setObjectOwnership(m_pItem, QQmlEngine::CppOwnership);
    m_pItem->setParentItem(q_ptr);
}


QVector<QByteArray>& MIWContextExtension::propertyNames() const
{
    static QVector<QByteArray> ret { "_modelIndexWatcher", };
    return ret;
}

QVariant MIWContextExtension::getProperty(AbstractItemAdapter* item, uint id, const QModelIndex& index) const
{
    Q_UNUSED(item)
    Q_UNUSED(index)
    Q_ASSERT(!id);
    return QVariant::fromValue(d_ptr->m_pWatcher);
}

#include <indexview.moc>
