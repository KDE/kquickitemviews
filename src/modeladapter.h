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

#include <QtCore/QObject>

//
class AbstractSelectableView;
class ContextManager;
class VisibleRange;
class AbstractQuickView;
class AbstractViewItem; //FIXME remove

// Qt
class QQmlComponent;
class QAbstractItemModel;

class ModelAdapterPrivate;

/**
 * Wrapper object to assign a model to a view.
 *
 * It supports both QSharedPointer based models and "raw" QAbstractItemModel.
 *
 * This class also allow multiple properties to be attached to the model, such
 * as a GUI delegate.
 */
class ModelAdapter : public QObject
{
    Q_OBJECT
public:
    Q_PROPERTY(QVariant model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QQmlComponent* delegate READ delegate WRITE setDelegate NOTIFY delegateChanged)
    Q_PROPERTY(bool empty READ isEmpty NOTIFY countChanged)

    /// The view can be collapsed
    Q_PROPERTY(bool collapsable READ isCollapsable WRITE setCollapsable)
    /// Expand all elements by default
    Q_PROPERTY(bool autoExpand READ isAutoExpand WRITE setAutoExpand)
    /// The maximum depth of a tree (for performance)
    Q_PROPERTY(int maxDepth READ maxDepth WRITE setMaxDepth)
    /// Recycle existing QQuickItem delegates for new QModelIndex (for performance)
    Q_PROPERTY(RecyclingMode recyclingMode READ recyclingMode WRITE setRecyclingMode)
    /// The number of elements to be preloaded outside of the visible area (for latency)
    Q_PROPERTY(int cacheBuffer READ cacheBuffer WRITE setCacheBuffer)
    /// The number of delegates to be kept in a recycling pool (for performance)
    Q_PROPERTY(int poolSize READ poolSize WRITE setPoolSize)

    enum RecyclingMode {
        NoRecycling    , /*!< Destroy and create new QQuickItems all the time         */
        RecyclePerDepth, /*!< Keep different pools buffer for each levels of children */
        AlwaysRecycle  , /*!< Assume all depth level use the same delegate            */
    };
    Q_ENUM(RecyclingMode)

    explicit ModelAdapter(AbstractQuickView* parent = nullptr);
    virtual ~ModelAdapter();

    QVariant model() const;
    void setModel(const QVariant& var);

    virtual void setDelegate(QQmlComponent* delegate);
    QQmlComponent* delegate() const;

    bool isCollapsable() const;
    void setCollapsable(bool value);

    bool isAutoExpand() const;
    void setAutoExpand(bool value);

    int maxDepth() const;
    void setMaxDepth(int depth);

    int cacheBuffer() const;
    void setCacheBuffer(int value);

    int poolSize() const;
    void setPoolSize(int value);

    RecyclingMode recyclingMode() const;
    void setRecyclingMode(RecyclingMode mode);

    bool isEmpty() const;

    AbstractSelectableView* selectionManager() const;
    ContextManager* contextManager() const;

    QVector<VisibleRange*> visibleRanges() const;

    QAbstractItemModel *rawModel() const;

    AbstractViewItem* itemForIndex(const QModelIndex& idx) const; //FIXME remove

protected:
    // Rather then scope-creeping this class, all selection related elements
    // are implemented independently.
    void setSelectionManager(AbstractSelectableView* v);

    // Rather then scope-creeping this class, all methods and logic related
    // to keeping the QQmlContext object in sync with the model are delegated
    // to this class.
    void setContextManager(ContextManager* rm);

Q_SIGNALS:
    void modelAboutToChange(QAbstractItemModel* m);
    void modelChanged(QAbstractItemModel* m);
    void countChanged();
    void delegateChanged(QQmlComponent* delegate);
    void contentChanged();

private:
    ModelAdapterPrivate *d_ptr;
};
