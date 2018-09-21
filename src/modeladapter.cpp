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
#include "modeladapter.h"

#include <QtCore/QVariant>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QSharedPointer>
#include <QtCore/QDebug>

#include "visiblerange.h"
#include "abstractquickview.h"
#include "abstractselectableview.h"
#include "abstractselectableview_p.h"
#include "treetraversalreflector_p.h"
#include "abstractviewitem_p.h"
#include "abstractviewitem.h"

using QSharedItemModel = QSharedPointer<QAbstractItemModel>;

class ModelAdapterPrivate : public QObject
{
    Q_OBJECT
public:
    enum class Mode {
        NONE,
        SMART_POINTER,
        RAW
    } m_Mode {Mode::NONE};

    QSharedItemModel         m_pModelPtr           {       };
    QAbstractItemModel      *m_pRawModel           {nullptr};
    QQmlComponent           *m_pDelegate           {nullptr};
    TreeTraversalReflector  *m_pReflector          {nullptr};
    VisibleRange            *m_pRange              {nullptr};
    AbstractSelectableView  *m_pSelectionManager   {nullptr};
    ContextManager          *m_pRoleContextManager {nullptr};

    bool m_Collapsable {true };
    bool m_AutoExpand  {false};
    int  m_MaxDepth    { -1  };
    int  m_CacheBuffer { 10  };
    int  m_PoolSize    { 10  };

    ModelAdapter::RecyclingMode m_RecyclingMode {
        ModelAdapter::RecyclingMode::NoRecycling
    };

    // Helpers
    void setModelCommon(QAbstractItemModel* m, QAbstractItemModel* old);

    ModelAdapter *q_ptr;

public Q_SLOTS:
    void slotContentChanged();
    void slotCountChanged();
    void slotDataChanged(const QModelIndex& tl, const QModelIndex& br); //TODO move to the range
};

ModelAdapter::ModelAdapter(AbstractQuickView* parent) : QObject(parent),
    d_ptr(new ModelAdapterPrivate())
{
    d_ptr->q_ptr = this;
    d_ptr->m_pSelectionManager = new AbstractSelectableView(this);

    d_ptr->m_pReflector = new TreeTraversalReflector(parent);
    selectionManager()->s_ptr->setView(parent);
    contextManager()->addPropertyGroup(selectionManager()->propertyGroup());

    d_ptr->m_pRange = new VisibleRange(this);
    d_ptr->m_pReflector->addRange(d_ptr->m_pRange);


    d_ptr->m_pReflector->setItemFactory([this, parent]() -> AbstractViewItem* {
        auto ret = parent->createItem(d_ptr->m_pRange);

        return ret;
    });

    connect(this, &ModelAdapter::delegateChanged,
        d_ptr->m_pReflector, &TreeTraversalReflector::resetEverything);
    connect(d_ptr->m_pReflector, &TreeTraversalReflector::contentChanged,
        d_ptr, &ModelAdapterPrivate::slotContentChanged);
    connect(d_ptr->m_pReflector, &TreeTraversalReflector::countChanged,
        d_ptr, &ModelAdapterPrivate::slotCountChanged);
}

ModelAdapter::~ModelAdapter()
{
    delete d_ptr;
}

QVariant ModelAdapter::model() const
{
    switch(d_ptr->m_Mode) {
        case ModelAdapterPrivate::Mode::SMART_POINTER:
            return QVariant::fromValue(d_ptr->m_pModelPtr);
        case ModelAdapterPrivate::Mode::RAW:
            return QVariant::fromValue(d_ptr->m_pRawModel);
        case ModelAdapterPrivate::Mode::NONE:
        default:
            return {};
    }
}

void ModelAdapterPrivate::setModelCommon(QAbstractItemModel* m, QAbstractItemModel* old)
{
    if (old)
        disconnect(old, &QAbstractItemModel::dataChanged, this,
            &ModelAdapterPrivate::slotDataChanged);

    m_pReflector->setModel(m);
    q_ptr->selectionManager()->s_ptr->setModel(m);
    m_pReflector->populate();

    if (m)
        connect(m, &QAbstractItemModel::dataChanged, this,
            &ModelAdapterPrivate::slotDataChanged);
}

void ModelAdapter::setModel(const QVariant& var)
{
    QSharedItemModel oldPtr;
    QAbstractItemModel* oldM = d_ptr->m_pRawModel;

    if (d_ptr->m_pModelPtr)
        oldPtr = d_ptr->m_pModelPtr;

    if (var.isNull()) {
        if ((!d_ptr->m_pRawModel) && (!d_ptr->m_pModelPtr))
            return;

        emit modelAboutToChange(nullptr);
        d_ptr->m_Mode = ModelAdapterPrivate::Mode::NONE;
        d_ptr->m_pRawModel = nullptr;
        d_ptr->m_pModelPtr = nullptr;
        d_ptr->setModelCommon(nullptr, oldPtr ? oldPtr.data() : oldM);
        emit modelChanged(nullptr);
    }
    else if (auto m = var.value<QSharedItemModel >()) {
        if (m == d_ptr->m_pModelPtr)
            return;

        // Use .data() is "safe", this call holds a reference, but the user
        // users should "not" store it. Obviously they all will, but at least
        // they have to watch the signal and replace it in time. If they don't,
        // then it may crash but the bug is on the consumer side.
        emit modelAboutToChange(m.data());
        d_ptr->m_Mode = ModelAdapterPrivate::Mode::SMART_POINTER;
        d_ptr->m_pRawModel = nullptr;
        d_ptr->m_pModelPtr = m;
        d_ptr->setModelCommon(m.data(), oldPtr ? oldPtr.data() : oldM);
        emit modelChanged(m.data());

    }
    else if (auto m = var.value<QAbstractItemModel*>()) {
        if (m == d_ptr->m_pRawModel)
            return;

        emit modelAboutToChange(m);
        d_ptr->m_Mode = ModelAdapterPrivate::Mode::RAW;
        d_ptr->m_pRawModel = m;
        d_ptr->m_pModelPtr = nullptr;
        d_ptr->setModelCommon(m, oldPtr ? oldPtr.data() : oldM);
        emit modelChanged(m);
    }
    else {
        Q_ASSERT(false);
        qWarning() << "Cannot convert" << var << "to a model";
        setModel({});
    }
}

void ModelAdapter::setDelegate(QQmlComponent* delegate)
{
    if (delegate == d_ptr->m_pDelegate)
        return;

    d_ptr->m_pDelegate = delegate;
    emit delegateChanged(delegate);
}

QQmlComponent* ModelAdapter::delegate() const
{
    return d_ptr->m_pDelegate;
}

bool ModelAdapter::isEmpty() const
{
    switch(d_ptr->m_Mode) {
        case ModelAdapterPrivate::Mode::SMART_POINTER:
            return d_ptr->m_pModelPtr->rowCount() == 0;
        case ModelAdapterPrivate::Mode::RAW:
            return d_ptr->m_pRawModel->rowCount() == 0;
        case ModelAdapterPrivate::Mode::NONE:
        default:
            return true;
    }
}

bool ModelAdapter::isCollapsable() const
{
    return d_ptr->m_Collapsable;
}

void ModelAdapter::setCollapsable(bool value)
{
    d_ptr->m_Collapsable = value;
}

bool ModelAdapter::isAutoExpand() const
{
    return d_ptr->m_AutoExpand;
}

void ModelAdapter::setAutoExpand(bool value)
{
    d_ptr->m_AutoExpand = value;
}

int ModelAdapter::maxDepth() const
{
    return d_ptr->m_MaxDepth;
}

void ModelAdapter::setMaxDepth(int depth)
{
    d_ptr->m_MaxDepth = depth;
}

int ModelAdapter::cacheBuffer() const
{
    return d_ptr->m_CacheBuffer;
}

void ModelAdapter::setCacheBuffer(int value)
{
    d_ptr->m_CacheBuffer = value;
}

int ModelAdapter::poolSize() const
{
    return d_ptr->m_PoolSize;
}

void ModelAdapter::setPoolSize(int value)
{
    d_ptr->m_PoolSize = value;
}

ModelAdapter::RecyclingMode ModelAdapter::recyclingMode() const
{
    return d_ptr->m_RecyclingMode;
}

void ModelAdapter::setRecyclingMode(ModelAdapter::RecyclingMode mode)
{
    d_ptr->m_RecyclingMode = mode;
}


void ModelAdapter::setSelectionManager(AbstractSelectableView* v)
{
    d_ptr->m_pSelectionManager = v;
}

AbstractSelectableView* ModelAdapter::selectionManager() const
{
    return d_ptr->m_pSelectionManager;
}

ContextManager* ModelAdapter::contextManager() const
{
    if (!d_ptr->m_pRoleContextManager)
        d_ptr->m_pRoleContextManager = new ContextManager();

    return d_ptr->m_pRoleContextManager;
}

void ModelAdapter::setContextManager(ContextManager* cm)
{
    // It cannot (yet) be replaced.
    Q_ASSERT(!d_ptr->m_pRoleContextManager);

    d_ptr->m_pRoleContextManager = cm;
}

QVector<VisibleRange*> ModelAdapter::visibleRanges() const
{
    return d_ptr->m_pReflector->ranges();
}

void ModelAdapterPrivate::slotContentChanged()
{
    emit q_ptr->contentChanged();
}

void ModelAdapterPrivate::slotCountChanged()
{
    emit q_ptr->countChanged();
}

QAbstractItemModel *ModelAdapter::rawModel() const
{
    switch(d_ptr->m_Mode) {
        case ModelAdapterPrivate::Mode::SMART_POINTER:
            return d_ptr->m_pModelPtr.data();
        case ModelAdapterPrivate::Mode::RAW:
            return d_ptr->m_pRawModel;
        case ModelAdapterPrivate::Mode::NONE:
        default:
            return nullptr;
    }
}

//TODO remove this content and check each range
void ModelAdapterPrivate::slotDataChanged(const QModelIndex& tl, const QModelIndex& br)
{
    if (tl.model() && tl.model() != q_ptr->rawModel()) {
        Q_ASSERT(false);
        return;
    }

    if (br.model() && br.model() != q_ptr->rawModel()) {
        Q_ASSERT(false);
        return;
    }

    if ((!tl.isValid()) || (!br.isValid()))
        return;

    if (!m_pReflector->isActive(tl.parent(), tl.row(), br.row()))
        return;

    //FIXME tolerate other cases
    Q_ASSERT(q_ptr->rawModel());
    Q_ASSERT(tl.model() == q_ptr->rawModel() && br.model() == q_ptr->rawModel());
    Q_ASSERT(tl.parent() == br.parent());

    //TODO Use a smaller range when possible

    //itemForIndex(const QModelIndex& idx) const final override;
    for (int i = tl.row(); i <= br.row(); i++) {
        const auto idx = q_ptr->rawModel()->index(i, tl.column(), tl.parent());
        if (auto item = m_pReflector->itemForIndex(idx))
            item->s_ptr->performAction(VisualTreeItem::ViewAction::UPDATE);
    }
}

AbstractViewItem* ModelAdapter::itemForIndex(const QModelIndex& idx) const
{
    return d_ptr->m_pReflector->itemForIndex(idx);
}

#include <modeladapter.moc>
