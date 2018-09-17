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
#include "sizehintproxymodel.h"

#include "contextmanager.h"

// Qt
#include <QJSValue>
#include <QDebug>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlExpression>

class SizeHintProxyModelPrivate : public QObject
{
public:


    QHash<QByteArray, int> m_hInvertedRoleNames;
    QStringList            m_lInvalidationRoles;
    QJSValue               m_ConstantsCallback;
    QJSValue               m_Contants;
    QQmlScriptString       m_WidthScript;
    QQmlScriptString       m_HeightScript;
    bool                   m_ReloadContants    { false };
    QQmlExpression*        m_pWidthExpression  {nullptr};
    QQmlExpression*        m_pHeightExpression {nullptr};
    QQmlContext           *m_pQmlContext       {nullptr};
    ContextManager        *m_pContextManager   {nullptr};
    ContextBuilder        *m_pContextBuilder   {nullptr};
    QVariantMap            m_hCache            {       };


    // Helpers
    int   roleIndex(const QString& name);
    void  reloadContants();
    void  reloadContext(QAbstractItemModel *m);
    qreal evaluateForIndex(QQmlExpression* expr, const QModelIndex& idx);

    SizeHintProxyModel* q_ptr;


public Q_SLOTS:
    void slotDataChanged(const QModelIndex& tl, const QModelIndex& bl, const QVector<int>& roles);
};

SizeHintProxyModel::SizeHintProxyModel(QObject* parent) : QIdentityProxyModel(parent),
    d_ptr(new SizeHintProxyModelPrivate())
{
    d_ptr->q_ptr = this;
}

SizeHintProxyModel::~SizeHintProxyModel()
{
    delete d_ptr;
}

void SizeHintProxyModel::setSourceModel(QAbstractItemModel *newSourceModel)
{

    d_ptr->reloadContext(newSourceModel);
    d_ptr->m_ReloadContants = true;
    d_ptr->m_hInvertedRoleNames.clear();
    QIdentityProxyModel::setSourceModel(newSourceModel);
    d_ptr->reloadContants();
}

QJSValue SizeHintProxyModel::constants() const
{
    return d_ptr->m_ConstantsCallback;
}

void SizeHintProxyModel::setConstants(const QJSValue& value)
{
    d_ptr->m_ReloadContants    = true;
    d_ptr->m_ConstantsCallback = value;
    d_ptr->reloadContants();
}

int SizeHintProxyModelPrivate::roleIndex(const QString& name)
{
    if (!q_ptr->sourceModel())
        return -1;

    const QByteArray n = name.toLatin1();

    if (m_hInvertedRoleNames.isEmpty()) {
        const QHash<int, QByteArray> roles = q_ptr->sourceModel()->roleNames();
        for (auto i = roles.constBegin(); i != roles.constEnd(); i++)
            m_hInvertedRoleNames.insert(i.value(), i.key());
    }

    return m_hInvertedRoleNames[n];
}

void SizeHintProxyModel::invalidateConstants()
{
    d_ptr->m_ReloadContants = true;
    d_ptr->m_hCache.clear();
}

QVariant SizeHintProxyModel::getRoleValue(const QModelIndex& idx, const QString& roleName) const
{
    return idx.data(d_ptr->roleIndex(roleName));
}

QStringList SizeHintProxyModel::invalidationRoles() const
{
    return d_ptr->m_lInvalidationRoles;
}

void SizeHintProxyModel::setInvalidationRoles(const QStringList& l)
{
    d_ptr->m_lInvalidationRoles = l;
}

void SizeHintProxyModelPrivate::slotDataChanged(const QModelIndex& tl, const QModelIndex& bl, const QVector<int>& roles)
{
    Q_UNUSED(tl)
    Q_UNUSED(bl)
    Q_UNUSED(roles)
    //TODO check m_lInvalidationRoles and add a sync interface with the view
}

void SizeHintProxyModelPrivate::reloadContext(QAbstractItemModel *m)
{
    if (m_pContextBuilder)
        delete m_pContextBuilder;

    if (m_pQmlContext) {
        delete m_pQmlContext;
        m_pQmlContext = nullptr;
    }

    if (m_pContextManager)
        delete m_pContextManager;

    m_pContextManager = new ContextManager();
    m_pContextManager->setModel(m);

    QQmlContext* qmlCtx = QQmlEngine::contextForObject(q_ptr);
    Q_ASSERT(qmlCtx && qmlCtx->engine());

    m_pContextBuilder = new ContextBuilder(m_pContextManager, qmlCtx, q_ptr);
    m_pContextBuilder->setCacheEnabled(false);
}

void SizeHintProxyModelPrivate::reloadContants()
{
    // Properties are initialized in a random order, this cannot be set before
    // the model.
    if (!q_ptr->sourceModel())
        return;

    if (!m_ReloadContants)
        return;

    // Allow both functions and direct values
    m_Contants = m_ConstantsCallback.isCallable() ?
        m_ConstantsCallback.call() : m_ConstantsCallback;

    const auto map = m_Contants.toVariant().toMap();

    // Replace the context because it's not allowed to write to internal
    // function contexts.
    if (!m_pQmlContext) {
        QQmlContext* qmlCtx = QQmlEngine::contextForObject(q_ptr);
        Q_ASSERT(qmlCtx && qmlCtx->engine());
        m_pQmlContext = m_pContextBuilder->context();
        Q_ASSERT(m_pQmlContext);
        QQmlEngine::setContextForObject(q_ptr, m_pQmlContext);
    }

    // Set them as context properties directly, no fancy QMetaType here.
    for (auto i = map.constBegin(); i != map.constEnd(); i++) {
        m_pQmlContext->setContextProperty(i.key(), i.value());
        q_ptr->setProperty(i.key().toLatin1(), i.value());
    }

    m_pHeightExpression = new QQmlExpression(
        m_HeightScript,
        m_pQmlContext,
        this
    );

    m_pWidthExpression = new QQmlExpression(
        m_WidthScript,
        m_pQmlContext,
        this
    );

    m_ReloadContants = false;
}

qreal SizeHintProxyModelPrivate::evaluateForIndex(QQmlExpression* expr, const QModelIndex& idx)
{
    reloadContants();

    bool valueIsUndefined = false;

    m_pContextBuilder->setModelIndex(idx);
    const QVariant var = expr->evaluate(&valueIsUndefined);//ret.toVariant();

    // There was an error in the expression
    if (expr->hasError()) {
        qWarning() << expr->error();
        return {};
    }
    if (valueIsUndefined && !var.isValid()) {
        qWarning() << "The sizeHint expression contains an undefined variable" << idx;
        return {};
    }
    else if (!var.isValid()) {
        qWarning() << "The sizeHint expression had an error" << idx;
        return {};
    }

    static QByteArray n("int"), n2("double");

    Q_ASSERT(QByteArray(var.typeName()) == n || QByteArray(var.typeName()) == n2);

    return var.toReal();
}

QSizeF SizeHintProxyModel::sizeHintForIndex(const QModelIndex& idx)
{
    const qreal w = d_ptr->m_pWidthExpression ?
        d_ptr->evaluateForIndex(d_ptr->m_pWidthExpression , idx) : 0;
    const qreal h = d_ptr->m_pHeightExpression ?
        d_ptr->evaluateForIndex(d_ptr->m_pHeightExpression, idx) : 0;
    return QSizeF {w, h};
}

QQmlScriptString SizeHintProxyModel::widthHint() const
{
    return d_ptr->m_WidthScript;
}

void SizeHintProxyModel::setWidthHint(const QQmlScriptString& value)
{
    if (d_ptr->m_pWidthExpression) {
        delete d_ptr->m_pWidthExpression;
        d_ptr->m_pWidthExpression = nullptr;
    }
    d_ptr->m_WidthScript = value;
    d_ptr->m_ReloadContants = true;
}

QQmlScriptString SizeHintProxyModel::heightHint() const
{
    return d_ptr->m_HeightScript;
}

void SizeHintProxyModel::setHeightHint(const QQmlScriptString& value)
{
    if (d_ptr->m_pHeightExpression) {
        delete d_ptr->m_pHeightExpression;
        d_ptr->m_pHeightExpression = nullptr;
    }

    d_ptr->m_HeightScript = value;
    d_ptr->m_ReloadContants = true;
}

