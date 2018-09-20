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

class ModelAdapterPrivate
{
public:
    enum class Mode {
        NONE,
        SMART_POINTER,
        RAW
    } m_Mode {Mode::NONE};

    QSharedPointer<QAbstractItemModel> m_pModelPtr;
    QAbstractItemModel *m_pRawModel {nullptr};
    QQmlComponent *m_pDelegate      {nullptr};
};

ModelAdapter::ModelAdapter(QObject* parent)
{

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

void ModelAdapter::setModel(const QVariant& var)
{

    if (var.isNull()) {
        if ((!d_ptr->m_pRawModel) && (!d_ptr->m_pModelPtr))
            return;

        emit modelAboutToChange(nullptr);
        d_ptr->m_Mode = ModelAdapterPrivate::Mode::NONE;
        d_ptr->m_pRawModel = nullptr;
        d_ptr->m_pModelPtr = nullptr;
        emit modelChanged(nullptr);
    }
    else if (auto m = var.value<QSharedPointer<QAbstractItemModel> >()) {
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
        emit modelChanged(m.data());

    }
    else if (auto m = var.value<QAbstractItemModel*>()) {
        if (m == d_ptr->m_pRawModel)
            return;

        emit modelAboutToChange(m);
        d_ptr->m_Mode = ModelAdapterPrivate::Mode::RAW;
        d_ptr->m_pRawModel = m;
        d_ptr->m_pModelPtr = nullptr;
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
