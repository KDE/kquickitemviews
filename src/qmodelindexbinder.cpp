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
#include "qmodelindexbinder.h"

// Qt
#include <QQmlEngine>
#include <QQmlContext>
#include <QtCore/QTimer>

// KQuickItemViews
#include "qmodelindexwatcher.h"
#include <adapters/contextadapter.h>

class QModelIndexBinderPrivate : public QObject
{
    Q_OBJECT
public:
    enum class Mode {
        CONTAINER,
        ATTACHED,
    };

    QByteArray          m_Role     {       };
    QByteArray          m_Prop     {       };
    QObject            *m_pContent {nullptr};
    int                 m_Delay    {   0   };
    QTimer             *m_pTimer   {nullptr};
    bool                m_AutoSave { true  };
    QQmlContext        *m_pCTX     {nullptr};
    ContextAdapter     *m_pAdapter {nullptr};
    QModelIndexWatcher *m_pWatcher {nullptr};
    bool                m_isBinded { false };
    Mode                m_Mode;

    // Helper
    void bind();

    QModelIndexBinder *q_ptr;

public Q_SLOTS:
    void loadWatcher();
    void slotModelPropChanged();
    void slotObjectPropChanged();
};

QModelIndexBinder::QModelIndexBinder(QQuickItem *parent) :
    QQuickItem(parent), d_ptr(new QModelIndexBinderPrivate())
{
    d_ptr->q_ptr = this;
    d_ptr->m_Mode = QModelIndexBinderPrivate::Mode::CONTAINER;

    //TODO actually track the context, it could be reparented
    QTimer::singleShot(0, d_ptr, SLOT(loadWatcher()));
}

QModelIndexBinder::QModelIndexBinder(QObject *parent) :
    QQuickItem(nullptr), d_ptr(new QModelIndexBinderPrivate())
{
    QObject::setParent(parent);

    d_ptr->q_ptr = this;
    d_ptr->m_Mode = QModelIndexBinderPrivate::Mode::ATTACHED;

    //TODO actually track the context, it could be reparented
    QTimer::singleShot(0, d_ptr, SLOT(loadWatcher()));
    _setObject(parent);
}


QModelIndexBinder::~QModelIndexBinder()
{
    delete d_ptr;
}

QString QModelIndexBinder::modelRole() const
{
    return d_ptr->m_Role;
}

void QModelIndexBinder::setModelRole(const QString& role)
{
    d_ptr->m_Role = role.toLatin1();
    emit changed();
    d_ptr->bind();
}

QString QModelIndexBinder::objectProperty() const
{
    return d_ptr->m_Prop;
}

void QModelIndexBinder::setObjectProperty(const QString& op)
{
    d_ptr->m_Prop = op.toLatin1();
}

QObject *QModelIndexBinder::_object() const
{
    return d_ptr->m_pContent;
}

void QModelIndexBinder::_setObject(QObject *o)
{
    // If the object is a QQuickItem, add it to the view
    if (d_ptr->m_Mode == QModelIndexBinderPrivate::Mode::CONTAINER) {
        if (auto w = qobject_cast<QQuickItem*>(o)) {
            w->setParentItem(this);
        }
    }

    d_ptr->m_pContent = o;
    emit changed();
    d_ptr->bind();
}

int QModelIndexBinder::delay() const
{
    return d_ptr->m_Delay;
}

void QModelIndexBinder::setDelay(int d)
{
    d_ptr->m_Delay = d;
    emit changed();
}

bool QModelIndexBinder::autoSave() const
{
    return d_ptr->m_AutoSave;
}

void QModelIndexBinder::setAutoSave(bool v)
{
    d_ptr->m_AutoSave = v;
    emit changed();
}

bool QModelIndexBinder::isSynchronized() const
{
    //TODO
    return true;
}

void QModelIndexBinder::reset() const
{
    //TODO
}

bool QModelIndexBinder::applyNow() const
{
    //TODO
    return true;
}

void QModelIndexBinderPrivate::loadWatcher()
{
    if (m_pWatcher)
        return;

    // Container mode
    m_pCTX = QQmlEngine::contextForObject(q_ptr);

    // Attached mode
    if ((!m_pCTX) || m_pContent)
        m_pCTX = QQmlEngine::contextForObject(m_pContent);

    Q_ASSERT(m_pCTX);

    if (!m_pCTX)
        return;

    auto v = m_pCTX->contextProperty("_modelIndexWatcher");
    auto c = m_pCTX->contextProperty("_contextAdapter");

    m_pAdapter = qvariant_cast<ContextAdapter*>(c);
    m_pWatcher = qobject_cast<QModelIndexWatcher*>(
        qvariant_cast<QObject*>(v)
    );

    Q_ASSERT(m_pAdapter);
    Q_ASSERT(m_pWatcher);

    bind();
}

void QModelIndexBinderPrivate::bind()
{
    if (m_isBinded || m_Role.isEmpty() || m_Prop.isEmpty() || (!m_pContent) || (!m_pWatcher) || (!m_pAdapter))
        return;

    const auto co = m_pAdapter->contextObject();

    // Find the properties on both side
    const int objPropId  = m_pContent->metaObject()->indexOfProperty(m_Prop);
    const int rolePropId = co->metaObject()->indexOfProperty(m_Role);

    if (rolePropId == -1)
        qWarning() << "Role" << m_Role << "not found";

    if (objPropId == -1)
        qWarning() << "Property" << m_Prop << "not found";

    Q_ASSERT(objPropId  != -1 && rolePropId != -1);

    auto metaProp = m_pContent->metaObject()->property(objPropId);
    auto metaRole = co->metaObject()->property(rolePropId);

    // Connect to the metaSlots
    auto metaSlotProp = metaObject()->method(metaObject()->indexOfMethod("slotObjectPropChanged()"));
    auto metaSlotRole = metaObject()->method(metaObject()->indexOfMethod("slotModelPropChanged()"));

    // Set the initial value before connecting
    slotModelPropChanged();

    connect(m_pContent, metaProp.notifySignal(), this, metaSlotProp);
    connect(co        , metaRole.notifySignal(), this, metaSlotRole);
}

void QModelIndexBinderPrivate::slotModelPropChanged()
{
    const auto role = m_pAdapter->contextObject()->property(m_Role);

    //HACK this is a bug in ContextAdapterFactory
    if (!role.isValid())
        return;

    const auto prop = m_pContent->property(m_Prop);

    // Some widgets may not try to detect if the value **really** changes and
    // emit signals anyway.
    if (role != m_pContent->property(m_Prop))
        m_pContent->setProperty(m_Prop, role);
}

void QModelIndexBinderPrivate::slotObjectPropChanged()
{
    const auto role = m_pAdapter->contextObject()->property(m_Role);

    Q_ASSERT(role.isValid());
    //HACK this is a bug in ContextAdapterFactory
    if (!role.isValid())
        return;

    const auto prop = m_pContent->property(m_Prop);

    if (role != prop)
        m_pAdapter->contextObject()->setProperty(m_Role, prop); //FIXME fix the model::setData support
}

QModelIndexBinder *QModelIndexBinder::qmlAttachedProperties(QObject *object)
{
    return new QModelIndexBinder(object);
}

#include <qmodelindexbinder.moc>
