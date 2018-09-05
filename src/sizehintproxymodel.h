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
#pragma once

#include <QtCore/QIdentityProxyModel>
#include <QtCore/QSize>
#include <QtCore/QVariant>
#include <QJSValue>


class SizeHintProxyModelPrivate;

/**
 * This proxy model allows to predict the final size of a view delegate.
 *
 * This model needs to be implemented when both of the following things are
 * used:
 *
 * * A scrollbar or anything that depends on `contentHeight` being correct
 * * A non uniform delegate size
 *
 * Back in the QtWidgets days, the QItemDelegate/QStyledItemDelegate had the
 * `sizeHint` method. It was easy to use because it was the same class that
 * performed the painting so it had all the necessary knowledge to return
 * the correct value. In QML, however, it's not the case. Predicting the size
 * may or may not require knowledge of the style that are not officially in
 * public APIs. That being said, it should usually be possible to gather the
 * data in one way or another.
 *
 * This class is part of the public C++ API so the `sizeHint` method can be
 * re-implemented. Otherwise it is specified in JavaScript.
 */
class Q_DECL_EXPORT SizeHintProxyModel : public QIdentityProxyModel
{
    Q_OBJECT
public:
    Q_PROPERTY(QJSValue callback READ callback WRITE setCallback)

    explicit SizeHintProxyModel(QObject* parent = nullptr);
    virtual ~SizeHintProxyModel();

    virtual void setSourceModel(QAbstractItemModel *newSourceModel) override;

    QJSValue callback() const;
    void setCallback(const QJSValue& value);

private:
    SizeHintProxyModelPrivate* d_ptr;
    Q_DECLARE_PRIVATE(SizeHintProxyModel)
};

Q_DECLARE_METATYPE(SizeHintProxyModel*)
