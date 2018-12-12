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
#ifndef LISTVIEW_H
#define LISTVIEW_H

// KQuickItemViews
#include <KQuickItemViews/singlemodelviewbase.h>
class ListViewPrivate;
class ListView;

// Qt
class QQuickItem;

/**
 * Equivalent of the QtQuick.ListView.Section class to keep the API mostly
 * compatible.
 *
 * A section model can be set. If it does, this class doesn't do *any* check if
 * the section model matches the sections. It is entirely the responsibility of
 * the programmer to provide a valid/compatible model.
 */
class Q_DECL_EXPORT ListViewSections : public QObject
{
    Q_OBJECT

    friend class ListView;
public:
    Q_PROPERTY(QQmlComponent* delegate READ delegate WRITE setDelegate)
    Q_PROPERTY(QString        property READ property WRITE setProperty)
    Q_PROPERTY(QStringList    roles    READ roles    WRITE setRoles   )
    Q_PROPERTY(int            role     READ role                      )

    Q_PROPERTY(QSharedPointer<QAbstractItemModel> model READ model WRITE setModel)

    explicit ListViewSections(ListView* parent);
    virtual ~ListViewSections();

    QQmlComponent* delegate() const;
    void setDelegate(QQmlComponent* component);

    QString property() const;
    void setProperty(const QString& property);

    QStringList roles() const;
    void setRoles(const QStringList& list);

    QSharedPointer<QAbstractItemModel> model() const;
    void setModel(const QSharedPointer<QAbstractItemModel>& m);

    int role() const;

private:
    ListViewPrivate* d_ptr;
};

Q_DECLARE_METATYPE(ListViewSections*)

/**
 * Re-implementation of QtQuick.ListView.
 *
 * Why:
 *
 *  * The original is not capable of generating a proper "table of content" and
 *    scrollbar.
 *  * The "section" support of the original system is limited to a single string,
 *    this is useless for some use cases.
 *  * The section didn't support getting the metadata, such as the number of
 *    entries (lazy loaded, of course).
 *  * Real drag and drop (QMimeData and models) are not supported
 *  * Models are generally badly supported.
 *  * The QtQuick.ListView is remotely close enough to a drop-in replacement for
 *    QtWidgets::QListView. Stable and mature models should not require modifications
 *    to acknowledge misguided QtQuick.ListView changes.
 */
class Q_DECL_EXPORT ListView : public SingleModelViewBase
{
    Q_OBJECT

    friend class ListViewItem;
    friend class ListViewSections;
public:
    Q_PROPERTY(ListViewSections* section READ section CONSTANT)
    Q_PROPERTY(int count READ count /*NOTIFY contentChanged*/)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY indexChanged)

    explicit ListView(QQuickItem* parent = nullptr);
    virtual ~ListView();

    ListViewSections* section() const;

    int count() const;

    int currentIndex() const;
    void setCurrentIndex(int index);

Q_SIGNALS:
    void indexChanged(int index);

private:

    ListViewPrivate* d_ptr;
    Q_DECLARE_PRIVATE(ListView)
};

#endif
