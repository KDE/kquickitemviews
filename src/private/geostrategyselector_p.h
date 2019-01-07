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

#include <adapters/geometryadapter.h>

//Qt
class QAbstractItemModel;

class GeoStrategySelectorPrivate;

/**
 * The "real" strategy adapter.
 *
 * It acts as a proxy with a built-in chain of responsibilities to select the
 * best geometry hints strategy automatically.
 *
 * For this, it uses runtime data introspection to auto-detect features that
 * allows to make a better strategy choice.
 */
class GeoStrategySelector final : public GeometryAdapter
{
    Q_OBJECT
public:

    /**
     * Auto detected and provided features.
     */
    enum Features {
        NONE               = 0x0 << 0 , /*!< No features to make en enlighten choice */
        HAS_MODEL          = 0x1 << 0 , /*!< Has a model                             */
        HAS_SHP_MODEL      = 0x1 << 1 , /*!< Has a QSizeHintProxyModel               */
        HAS_MODEL_CONTENT  = 0x1 << 2 , /*!< Has a model with content to introspect  */
        HAS_SIZE_ROLE      = 0x1 << 3 , /*!< Has a model with valid size hints       */
        HAS_DELEGATE       = 0x1 << 4 , /*!< Has a delegate instance                 */
        HAS_OVERRIDE       = 0x1 << 5 , /*!< A GeometryAdapter was manually set      */
        HAS_SCROLLBAR      = 0x1 << 6 , /*!< The view need the full geometry         */
        HAS_CHILDREN       = 0x1 << 7 , /*!< The model only has top level            */
        HAS_COLUMNS        = 0x1 << 8 , /*!< The model has more than 1 column        */
        HAS_MAX_DEPTH      = 0x1 << 9 , /*!< There is a known maximum recursion      */
        IS_FULLY_COLLAPSED = 0x1 << 10, /*!< It is a tree, but nothing is expanded   */
    };
    Q_FLAGS(Features)

    explicit GeoStrategySelector(Viewport *parent = nullptr);
    virtual ~GeoStrategySelector();

    virtual QSizeF sizeHint(const QModelIndex& index, AbstractItemAdapter *adapter) const override;
    virtual QPointF positionHint(const QModelIndex& index, AbstractItemAdapter *adapter) const override;

    virtual int capabilities() const override;

    void setModel(QAbstractItemModel *m);

    void setHasScrollbar(bool v);

    /// Return true when the currentAdapter is selected using the capabilities.
    bool isAutomatic() const;

    GeometryAdapter *currentAdapter() const;
    void setCurrentAdapter(GeometryAdapter *a);

private:
    GeoStrategySelectorPrivate *d_ptr;
    Q_DECLARE_PRIVATE(GeoStrategySelector)
};
