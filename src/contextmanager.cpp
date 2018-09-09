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
#include "contextmanager.h"

#include <QtCore/QAbstractItemModel>
#include <QtCore/private/qmetaobjectbuilder_p.h>
#include <QQmlContext>
#include <QQuickItem>

class ContextManagerPrivate
{
public:
    QAbstractItemModel* m_pModel {nullptr};
    mutable QHash<int, QString> m_hRoleNames;
    mutable QHash<const QAbstractItemModel*, QHash<int, QString>*> m_hhOtherRoleNames;

    QHash<int, QString>* reloadRoleNames(const QModelIndex& index) const;

    ContextManager* q_ptr;
};

ContextManager::ContextManager(QObject* parent) : QObject(parent),
    d_ptr(new ContextManagerPrivate())
{
    d_ptr->q_ptr = this;
}

ContextManager::~ContextManager()
{
    delete d_ptr;
}

void ContextManager::setModel(QAbstractItemModel* m)
{
    d_ptr->m_pModel = m;
}

QAbstractItemModel* ContextManager::model() const
{
    return d_ptr->m_pModel;
}

/**
 * This helper method convert the model role names (QByteArray) to QML context
 * properties (QString) only once.
 *
 * If this wasn't done, it would cause millions of QByteArray->QString temporary
 * allocations.
 */
QHash<int, QString>* ContextManagerPrivate::reloadRoleNames(const QModelIndex& index) const
{
    if (!q_ptr->model())
        return nullptr;

    const auto m = index.model() ? index.model() : q_ptr->model();

    auto* hash = m == q_ptr->model() ? &m_hRoleNames : m_hhOtherRoleNames.value(m);

    if (!hash)
        m_hhOtherRoleNames[m] = hash = new QHash<int, QString>;

    hash->clear();

    const auto roleNames = m->roleNames();

    for (auto i = roleNames.constBegin(); i != roleNames.constEnd(); ++i)
        (*hash)[i.key()] = i.value();

    return hash;
}


void ContextManager::applyRoles(QQmlContext* ctx, const QModelIndex& self) const
{
    auto m = self.model();

    if (Q_UNLIKELY(!m))
        return;

    auto* hash = m == d_ptr->m_pModel ?
        &d_ptr->m_hRoleNames : d_ptr->m_hhOtherRoleNames.value(m);

    // Refresh the cache
    if ((!hash) || model()->roleNames().size() != hash->size())
        hash = d_ptr->reloadRoleNames(self);

    Q_ASSERT(hash);

    // Add all roles to the
    for (auto i = hash->constBegin(); i != hash->constEnd(); ++i)
        ctx->setContextProperty(i.value() , self.data(i.key()));

    // Set extra index to improve ListView compatibility
    ctx->setContextProperty(QStringLiteral("index"        ) , self.row()        );
    ctx->setContextProperty(QStringLiteral("rootIndex"    ) , self              );
    ctx->setContextProperty(QStringLiteral("rowCount"     ) , m->rowCount(self) );
}
