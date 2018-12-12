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
#ifndef VIEWBASE_H
#define VIEWBASE_H

// KQuickItemViews
#include <KQuickItemViews/views/flickable.h>

// Qt
#include <QtCore/QAbstractItemModel>
#include <QtCore/QItemSelectionModel>
class QQmlComponent;

// KQuickItemViews
class ViewBasePrivate;
class AbstractItemAdapter;
class ModelAdapter;
class Viewport;

/**
 * Second generation of QtQuick treeview.
 *
 * The first one was designed for the chat view. It had a limited number of
 * requirement when it came to QtModel. However it required total control of
 * the layout.
 *
 * This is the opposite use case. The layout is classic, but the model support
 * has to be complete. Performance and lazy loading is also more important.
 *
 * It require less work to write a new treeview than refector the first one to
 * support the additional requirements. In the long run, the first generation
 * could be folded into this widget (if it ever makes sense, otherwise they will
 * keep diverging).
 */
class Q_DECL_EXPORT ViewBase : public Flickable
{
    Q_OBJECT
public:
    struct ItemFactoryBase {
        virtual AbstractItemAdapter* create(Viewport* r) const = 0;
        virtual ~ItemFactoryBase() {}
    };

    template<typename T> struct ItemFactory final : public ItemFactoryBase {
        virtual AbstractItemAdapter* create(Viewport* r) const override {
            return new T(r);
        }
    };

    Q_PROPERTY(bool empty READ isEmpty NOTIFY contentChanged)
    Q_PROPERTY(Qt::Corner gravity READ gravity WRITE setGravity)

    Qt::Corner gravity() const;
    void setGravity(Qt::Corner g);

    explicit ViewBase(QQuickItem* parent = nullptr);

    virtual ~ViewBase();

    QVector<ModelAdapter*> modelAdapters() const;

    bool isEmpty() const;

protected:
    virtual void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;


    /// To be used with moderation. Necessary when the delegate is replaced.
    void reload();

    /**
     * Extend this function when tasks need to be taken care of when an
     * exlicit refresh is needed. Remember to call the parent class
     * ::refresh() from the extended one.
     *
     * It is called, for example, when the model change.
     */
    virtual void refresh() final;

    void addModelAdapter(ModelAdapter* a);
    void removeModelAdapter(ModelAdapter* a);

Q_SIGNALS:
    void contentChanged();

private:
    ViewBasePrivate* d_ptr;
    Q_DECLARE_PRIVATE(ViewBase)
};

Q_DECLARE_METATYPE(QSharedPointer<QItemSelectionModel>)

#endif
