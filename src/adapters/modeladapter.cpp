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

// Qt
#include <QtCore/QVariant>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QSharedPointer>
#include <QtCore/QDebug>

// KQuickItemViews
#include "viewport.h"
#include "viewbase.h"
#include "selectionadapter.h"
#include "contextadapterfactory.h"
#include "private/selectionadapter_p.h"
#include "private/statetracker/viewitem_p.h"
#include "private/statetracker/content_p.h"
#include "private/statetracker/model_p.h"
#include "private/viewport_p.h"
#include "abstractitemadapter.h"
#include "proxies/sizehintproxymodel.h"

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

    QSharedItemModel        m_pModelPtr           {       };
    QAbstractItemModel     *m_pRawModel           {nullptr};
    QQmlComponent          *m_pDelegate           {nullptr};
    Viewport               *m_pViewport           {nullptr};
    SelectionAdapter       *m_pSelectionManager   {nullptr};
    ViewBase               *m_pView               {nullptr};
    ContextAdapterFactory  *m_pRoleContextFactory {nullptr};

    bool m_Collapsable {true };
    bool m_AutoExpand  {false};
    int  m_MaxDepth    { -1  };
    int  m_CacheBuffer { 10  };
    int  m_PoolSize    { 10  };

    int m_ExpandedCount { 999 }; //TODO

    ModelAdapter::RecyclingMode m_RecyclingMode {
        ModelAdapter::RecyclingMode::NoRecycling
    };

    // Helpers
    void setModelCommon(QAbstractItemModel* m, QAbstractItemModel* old);

    ModelAdapter *q_ptr;

public Q_SLOTS:
    void slotContentChanged();
    void slotDestroyed();
};

ModelAdapter::ModelAdapter(ViewBase* parent) : QObject(parent),
    d_ptr(new ModelAdapterPrivate())
{
    Q_ASSERT(parent);
    d_ptr->q_ptr = this;
    d_ptr->m_pView = parent;
    d_ptr->m_pSelectionManager = new SelectionAdapter(this);

    selectionAdapter()->s_ptr->setView(parent);
    contextAdapterFactory()->addContextExtension(selectionAdapter()->contextExtension());

    selectionAdapter()->s_ptr->setViewport(d_ptr->m_pViewport = new Viewport(this));

    connect(d_ptr->m_pViewport, &Viewport::contentChanged,
        d_ptr, &ModelAdapterPrivate::slotContentChanged);
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
        disconnect(old, &QObject::destroyed, this, &ModelAdapterPrivate::slotDestroyed);

    q_ptr->selectionAdapter()->s_ptr->setModel(m);

    if (auto f = m_pRoleContextFactory)
        f->setModel(m);

    if (m)
        connect(m, &QObject::destroyed, this, &ModelAdapterPrivate::slotDestroyed);
}

void ModelAdapter::setModel(const QVariant& var)
{
    QSharedItemModel oldPtr;
    QAbstractItemModel* oldM = oldPtr ? oldPtr.data() : d_ptr->m_pRawModel;

    if (d_ptr->m_pModelPtr)
        oldPtr = d_ptr->m_pModelPtr;

    if (var.isNull()) {
        if ((!d_ptr->m_pRawModel) && (!d_ptr->m_pModelPtr))
            return;

        emit modelAboutToChange(nullptr, oldM);
        d_ptr->m_Mode = ModelAdapterPrivate::Mode::NONE;
        d_ptr->m_pRawModel = nullptr;
        d_ptr->m_pModelPtr = nullptr;
        d_ptr->setModelCommon(nullptr, oldM);
        emit modelChanged(nullptr, oldM);
    }
    else if (auto m = var.value<QSharedItemModel >()) {
        if (m == d_ptr->m_pModelPtr)
            return;

        // Use .data() is "safe", this call holds a reference, but the user
        // users should "not" store it. Obviously they all will, but at least
        // they have to watch the signal and replace it in time. If they don't,
        // then it may crash but the bug is on the consumer side.
        emit modelAboutToChange(m.data(), oldM);
        d_ptr->m_Mode = ModelAdapterPrivate::Mode::SMART_POINTER;
        d_ptr->m_pRawModel = nullptr;
        d_ptr->m_pModelPtr = m;
        d_ptr->setModelCommon(m.data(), oldM);
        emit modelChanged(m.data(), oldM);

    }
    else if (auto m = var.value<QAbstractItemModel*>()) {
        if (m == d_ptr->m_pRawModel)
            return;

        emit modelAboutToChange(m, oldM);
        d_ptr->m_Mode = ModelAdapterPrivate::Mode::RAW;
        d_ptr->m_pRawModel = m;
        d_ptr->m_pModelPtr = nullptr;
        d_ptr->setModelCommon(m, oldPtr ? oldPtr.data() : oldM);
        emit modelChanged(m, oldM);
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

    if (d_ptr->m_Mode != ModelAdapterPrivate::Mode::NONE && d_ptr->m_pViewport) {
        //FIXME handle this properly
        auto content = d_ptr->m_pViewport->s_ptr->m_pReflector;

        content->modelTracker()
            << StateTracker::Model::Action::POPULATE
            << StateTracker::Model::Action::ENABLE;
    }
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
    d_ptr->m_CacheBuffer = std::min(1, value);
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

void ModelAdapter::setSelectionAdapter(SelectionAdapter* v)
{
    d_ptr->m_pSelectionManager = v;
}

SelectionAdapter* ModelAdapter::selectionAdapter() const
{
    return d_ptr->m_pSelectionManager;
}

ContextAdapterFactory* ModelAdapter::contextAdapterFactory() const
{
    if (!d_ptr->m_pRoleContextFactory) {
        d_ptr->m_pRoleContextFactory = new ContextAdapterFactory();

        if (auto m = rawModel())
            d_ptr->m_pRoleContextFactory->setModel(m);
    }

    return d_ptr->m_pRoleContextFactory;
}

QVector<Viewport*> ModelAdapter::viewports() const
{
    return {d_ptr->m_pViewport};
}

void ModelAdapterPrivate::slotContentChanged()
{
    emit q_ptr->contentChanged();
}

void ModelAdapterPrivate::slotDestroyed()
{
    q_ptr->setModel(
        QVariant::fromValue((QAbstractItemModel*)nullptr)
    );
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

ViewBase *ModelAdapter::view() const
{
    return d_ptr->m_pView;
}

bool ModelAdapter::isCollapsed() const
{
    // Having no expanded elements allows some optimizations to kick-in such
    // as total size using multiplications when the size is uniform.
    return d_ptr->m_ExpandedCount == 0;
}

#include <modeladapter.moc>
