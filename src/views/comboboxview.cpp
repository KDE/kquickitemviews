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
#include "comboboxview.h"

// Qt
#include <QQmlEngine>
#include <QQmlContext>
#include <QQuickWindow>

class ComboBoxViewPrivate final : public QObject
{
    Q_OBJECT
public:
    QItemSelectionModel* m_pSelectionModel {nullptr};
    QQuickItem*          m_pItem           {nullptr};

    ComboBoxView* q_ptr;

public Q_SLOTS:
    void slotWindowChanged(QQuickWindow *window);
    void slotActivated(int index);
    void slotCurrentChanged(const QModelIndex& idx);
    void slotResize();
};

ComboBoxView::ComboBoxView(QQuickItem* parent) : QQuickItem(parent),
    d_ptr(new ComboBoxViewPrivate)
{
    d_ptr->q_ptr = this;

    connect(this, &QQuickItem::windowChanged,
        d_ptr, &ComboBoxViewPrivate::slotWindowChanged);

    setHeight(40);
}

ComboBoxView::~ComboBoxView()
{
    if (d_ptr->m_pItem)
        delete d_ptr->m_pItem;

    delete d_ptr;
}

QItemSelectionModel* ComboBoxView::selectionModel() const
{
    return d_ptr->m_pSelectionModel;
}

void ComboBoxView::setSelectionModel(QItemSelectionModel* s)
{
    if (s && d_ptr->m_pSelectionModel && d_ptr->m_pSelectionModel != s)
        disconnect(d_ptr->m_pSelectionModel, &QItemSelectionModel::currentChanged,
            d_ptr, &ComboBoxViewPrivate::slotCurrentChanged);

    d_ptr->m_pSelectionModel = s;

    if (s && d_ptr->m_pItem) {
        d_ptr->blockSignals(true);
        d_ptr->m_pItem->setProperty("model", QVariant::fromValue(s->model()));
        if (s->currentIndex().isValid())
            d_ptr->m_pItem->setProperty("currentIndex", s->currentIndex().row());
        d_ptr->blockSignals(false);
    }

    connect(d_ptr->m_pSelectionModel, &QItemSelectionModel::currentChanged,
        d_ptr, &ComboBoxViewPrivate::slotCurrentChanged);
}

void ComboBoxViewPrivate::slotWindowChanged(QQuickWindow *window)
{
    if (!window)
        return;

    if (!m_pItem) {
        QQmlEngine *engine = QQmlEngine::contextForObject(q_ptr)->engine();

        Q_ASSERT(engine);

        QQmlComponent cbb(engine, q_ptr);
        cbb.setData("import QtQuick 2.4; import QtQuick.Controls 2.0;"\
        "ComboBox {textRole: \"display\"; anchors.fill: parent;}", {});
        m_pItem = qobject_cast<QQuickItem*>(cbb.create());

        if (m_pSelectionModel)
            q_ptr->setSelectionModel(m_pSelectionModel);

        connect(m_pItem, SIGNAL(activated(int)),
            this, SLOT(slotActivated(int)));

        connect(m_pItem, SIGNAL(implicitWidthChanged()),
            this, SLOT(slotResize()));

        q_ptr->setHeight(m_pItem->height());
        m_pItem->setParentItem(q_ptr);
        engine->setObjectOwnership(m_pItem, QQmlEngine::CppOwnership);
        slotResize();
    }
}

void ComboBoxViewPrivate::slotResize()
{
    q_ptr->setImplicitWidth(m_pItem->implicitWidth());
    q_ptr->setImplicitHeight(m_pItem->implicitHeight());
}

void ComboBoxViewPrivate::slotActivated(int index)
{
    if (!m_pSelectionModel)
        return;

    auto m = m_pSelectionModel->model();

    Q_ASSERT(m);
    Q_ASSERT(index >= 0 && index < m->rowCount());

    if (m_pSelectionModel)
        m_pSelectionModel->setCurrentIndex(
            m->index(index, 0),QItemSelectionModel::ClearAndSelect
        );

    slotResize();
}

void ComboBoxViewPrivate::slotCurrentChanged(const QModelIndex& idx)
{
    if (!m_pItem)
        return;

    if (!idx.isValid())
        return;

    m_pItem->setProperty("currentIndex", idx.row());
}

#include <comboboxview.moc>
