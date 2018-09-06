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

#include <QJSValue>
#include <QDebug>
#include <QQmlEngine>

class SizeHintProxyModelPrivate : public QObject
{
public:
    QJSValue m_Callback;
    QJSValue m_ContextCallback;
    QJSValue m_Context;
    bool m_ReloadContext {false};
    QHash<QByteArray, int> m_hInvertedRoleNames;
    QStringList m_lInvalidationRoles;

    int roleIndex(const QString& name);

    QVariantMap m_hCache;

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
    d_ptr->m_ReloadContext = true;
    d_ptr->m_hInvertedRoleNames.clear();
    QIdentityProxyModel::setSourceModel(newSourceModel);
}

QJSValue SizeHintProxyModel::sizeHint() const
{
    return d_ptr->m_Callback;
}

void SizeHintProxyModel::setSizeHint(const QJSValue& value)
{
    d_ptr->m_Callback = value;
}

QJSValue SizeHintProxyModel::context() const
{
    return d_ptr->m_ContextCallback;
}

void SizeHintProxyModel::setContext(const QJSValue& value)
{
    d_ptr->m_ReloadContext   = true;
    d_ptr->m_ContextCallback = value;
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

void SizeHintProxyModel::invalidateContext()
{
    d_ptr->m_ReloadContext = true;
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
    //TODO check m_lInvalidationRoles and add a sync interface with the view
}

QSizeF SizeHintProxyModel::sizeHintForIndex(QQmlEngine *engine, const QModelIndex& idx)
{
    if (!sizeHint().isCallable())
        return {};

    if (d_ptr->m_ReloadContext) {
        d_ptr->m_Context = context().call();
    }

    const auto args = engine->toScriptValue<QModelIndex>(idx);
    const auto ret = sizeHint().callWithInstance(
        d_ptr->m_Context, QJSValueList { args }
    );

    if (ret.isError()) {
        qDebug() << "Size hint failed" << ret.toString();
        Q_ASSERT(false);
    }

    const auto var = ret.toVariant();

    Q_ASSERT((!var.isValid()) || var.type() == QMetaType::QVariantList);

    const auto list = var.toList();
    Q_ASSERT(list.size() >= 2);

    return QSize {list[0].toReal(), list[1].toReal()};
}
