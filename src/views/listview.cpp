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
#include "listview.h"

// KQuickItemViews
#include "adapters/abstractitemadapter.h"
#include "adapters/contextadapter.h"
class ListViewItem;

// Qt
#include <QQmlEngine>
#include <QQmlContext>
#include <QtCore/QItemSelectionModel>


/**
 * Holds the metadata associated with a section.
 *
 * A section is created when a ListViewItem has a property that differs
 * from the previous ListViewItem in the list. It can also cross reference
 * into an external model to provide more flexibility.
 */
struct ListViewSection final
{
    explicit ListViewSection(
        ListViewItem* owner,
        const QVariant&    value
    );
    virtual ~ListViewSection();

    QQuickItem*      m_pItem    {nullptr};
    QQmlContext*     m_pContent {nullptr};
    int              m_Index    {   0   };
    int              m_RefCount {   1   };
    QVariant         m_Value    {       };
    ListViewSection* m_pPrevious{nullptr};
    ListViewSection* m_pNext    {nullptr};

    ListViewPrivate* d_ptr {nullptr};

    // Mutator
    QQuickItem* item(QQmlComponent* component);

    // Helpers
    void reparentSection(ListViewItem* newParent, ViewBase* view);

    // Getter
    ListViewItem *owner() const;

    // Setter
    void setOwner(ListViewItem *o);

private:
    ListViewItem *m_pOwner {nullptr};
};

class ListViewItem : public AbstractItemAdapter
{
public:
    explicit ListViewItem(Viewport* r);
    virtual ~ListViewItem();

    // Actions
    virtual bool attach () override;
    virtual bool move   () override;
    virtual bool remove () override;

    ListViewSection* m_pSection {nullptr};

    // Setters
    ListViewSection* setSection(ListViewSection* s, const QVariant& val);

    ListViewPrivate* d() const;
};

class ListViewPrivate : public QObject
{
    Q_OBJECT
public:
    explicit ListViewPrivate(ListView* p) : QObject(p), q_ptr(p){}

    // When all elements are assumed to have the same height, life is easy
    QVector<qreal>         m_DepthChart {   0   };
    ListViewSections* m_pSections  {nullptr};

    // Sections
    QQmlComponent* m_pDelegate   {nullptr};
    QString        m_Property    {       };
    QStringList    m_Roles       {       };
    int            m_CachedRole  {   0   };
    mutable bool   m_IndexLoaded { false };
    ListViewSection* m_pFirstSection {nullptr};
    QSharedPointer<QAbstractItemModel> m_pSectionModel;

    // Helpers
    ListViewSection* getSection(ListViewItem* i);
    void reloadSectionIndices() const;

    ListView* q_ptr;

public Q_SLOTS:
    void slotCurrentIndexChanged(const QModelIndex& index);
    void slotDataChanged(const QModelIndex& tl, const QModelIndex& br);
};

ListView::ListView(QQuickItem* parent) : SingleModelViewBase(new ItemFactory<ListViewItem>(), parent),
    d_ptr(new ListViewPrivate(this))
{
    connect(this, &SingleModelViewBase::currentIndexChanged,
        d_ptr, &ListViewPrivate::slotCurrentIndexChanged);
}

ListViewItem::~ListViewItem()
{
    // If this item is the section owner, assert before crashing
    if (m_pSection && m_pSection->owner() == this) {
        Q_ASSERT(false); //TODO
    }
}

ListView::~ListView()
{
    // Delete the sections
    while(auto sec = d_ptr->m_pFirstSection)
        delete sec;

    if (d_ptr->m_pSections)
        delete d_ptr->m_pSections;
}

int ListView::count() const
{
    return rawModel() ? rawModel()->rowCount() : 0;
}

int ListView::currentIndex() const
{
    return selectionModel()->currentIndex().row();
}

void ListView::setCurrentIndex(int index)
{
    if (!rawModel())
        return;

    SingleModelViewBase::setCurrentIndex(
        rawModel()->index(index, 0),
        QItemSelectionModel::ClearAndSelect
    );
}

void ListView::applyModelChanges(QAbstractItemModel* m)
{
    if (auto oldM = rawModel())
        disconnect(oldM, &QAbstractItemModel::dataChanged, d_ptr,
            &ListViewPrivate::slotDataChanged);

    SingleModelViewBase::applyModelChanges(m);

    if (!m)
        return;

    connect(m, &QAbstractItemModel::dataChanged, d_ptr,
        &ListViewPrivate::slotDataChanged);
}

ListViewSections* ListView::section() const
{
    if (!d_ptr->m_pSections) {
        d_ptr->m_pSections = new ListViewSections(
            const_cast<ListView*>(this)
        );
    }

    return d_ptr->m_pSections;
}

ListViewSection::ListViewSection(ListViewItem* owner, const QVariant& value) :
    m_pOwner(owner), m_Value(value), d_ptr(owner->d())
{
    m_pContent = new QQmlContext(owner->view()->rootContext());
    m_pContent->setContextProperty("section", value);
    owner->setBorderDecoration(Qt::TopEdge, 45.0);
}

QQuickItem* ListViewSection::item(QQmlComponent* component)
{
    if (m_pItem)
        return m_pItem;

    m_pItem = qobject_cast<QQuickItem*>(component->create(
        m_pContent
    ));

    m_pItem->setParentItem(owner()->view()->contentItem());

    return m_pItem;
}

ListViewSection* ListViewItem::setSection(ListViewSection* s, const QVariant& val)
{
    if ((!s) || s->m_Value != val)
        return nullptr;

    const auto p = static_cast<ListViewItem*>(next(Qt::TopEdge));
    const auto n = static_cast<ListViewItem*>(next(Qt::BottomEdge));

    // Garbage collect or change the old section owner
    if (m_pSection) {
        if (--m_pSection->m_RefCount <= 0)
            delete m_pSection;
        else if (p && n && p->m_pSection && p->m_pSection != m_pSection && n->m_pSection == m_pSection)
            m_pSection->setOwner(n);
        else if (p && p->m_pSection != m_pSection)
            Q_ASSERT(false); // There is a bug somewhere else
    }

    m_pSection = s;
    s->m_RefCount++;

    if ((!p) || p->m_pSection != s)
        s->setOwner(this);

    return s;
}

static void applyRoles(QQmlContext* ctx, const QModelIndex& self)
{
    auto m = self.model();

    if (Q_UNLIKELY(!m))
        return;

    Q_ASSERT(self.model());
    const auto hash = self.model()->roleNames();

    // Add all roles to the
    for (auto i = hash.constBegin(); i != hash.constEnd(); ++i)
        ctx->setContextProperty(i.value() , self.data(i.key()));

    // Set extra index to improve ListView compatibility
    ctx->setContextProperty(QStringLiteral("index"        ) , self.row()        );
    ctx->setContextProperty(QStringLiteral("rootIndex"    ) , self              );
    ctx->setContextProperty(QStringLiteral("rowCount"     ) , m->rowCount(self) );
}

/**
 * No lookup is performed, it is based on the previous entry and nothing else.
 *
 * This view only supports list. If it's with a tree, it will break and "don't
 * do this".
 */
ListViewSection* ListViewPrivate::getSection(ListViewItem* i)
{
    if (m_pSections->property().isEmpty() || !m_pDelegate)
        return nullptr;

    const auto val = q_ptr->rawModel()->data(i->index(), m_pSections->role());

    if (i->m_pSection && i->m_pSection->m_Value == val)
        return i->m_pSection;

    const auto prev = static_cast<ListViewItem*>(i->next(Qt::TopEdge));
    const auto next = static_cast<ListViewItem*>(i->next(Qt::BottomEdge));

    // The section owner isn't currently loaded
    if ((!prev) && i->row() > 0) {
        //Q_ASSERT(false); //TODO when GC is enabled, the assert is to make sure I don't forget
        return nullptr;
    }

    // Check if the nearby sections are compatible
    for (auto& s : {
        prev ? prev->m_pSection : nullptr, m_pFirstSection, next ? next->m_pSection : nullptr
    }) if (auto ret = i->setSection(s, val))
            return ret;

    // Create a section
    i->m_pSection = new ListViewSection(i, val);
    Q_ASSERT(i->m_pSection->m_RefCount == 1);

    // Update the double linked list
    if (prev && prev->m_pSection) {

        if (prev->m_pSection->m_pNext) {
            prev->m_pSection->m_pNext->m_pPrevious = i->m_pSection;
            i->m_pSection->m_pNext = prev->m_pSection->m_pNext;
        }

        prev->m_pSection->m_pNext  = i->m_pSection;
        i->m_pSection->m_pPrevious = prev->m_pSection;
        i->m_pSection->m_Index     = prev->m_pSection->m_Index + 1;

        Q_ASSERT(prev->m_pSection != prev->m_pSection->m_pNext);
        Q_ASSERT(i->m_pSection->m_pPrevious !=  i->m_pSection);
    }

    m_pFirstSection = m_pFirstSection ?
        m_pFirstSection : i->m_pSection;

    Q_ASSERT(m_pFirstSection);

    if (m_pSectionModel && !m_IndexLoaded)
        reloadSectionIndices();

    if (m_pSectionModel) {
        const auto idx = m_pSectionModel->index(i->m_pSection->m_Index, 0);
        Q_ASSERT((!idx.isValid()) || idx.model() == m_pSectionModel);

        //note: If you wish to fork this class, you can create a second context
        // manager and avoid the private API. Given it is available, this isn't
        // done here.
        applyRoles( i->m_pSection->m_pContent, idx);
    }

    // Create the item *after* applyRoles to avoid O(N) number of reloads
    q_ptr->rootContext()->engine()->setObjectOwnership(
        i->m_pSection->item(m_pDelegate), QQmlEngine::CppOwnership
    );

    return i->m_pSection;
}

/**
 * Set indices for each sections to the right value.
 */
void ListViewPrivate::reloadSectionIndices() const
{
    int idx = 0;
    for (auto i = m_pFirstSection; i; i = i->m_pNext) {
        Q_ASSERT(i != i->m_pNext);
        i->m_Index = idx++;
    }

    //FIXME this assumes all sections are always loaded, this isn't correct

    m_IndexLoaded = m_pFirstSection != nullptr;
}

ListViewItem::ListViewItem(Viewport* r) : AbstractItemAdapter(r)
{
}

ListViewPrivate* ListViewItem::d() const
{
    return static_cast<ListView*>(view())->ListView::d_ptr;
}

bool ListViewItem::attach()
{
    // This will trigger the lazy-loading of the item
    if (!item())
        return false;

    context()->setContextProperty("isCurrentItem", false);
    context()->setContextProperty("modelIndex", index());

    // When the item resizes itself
    QObject::connect(item(), &QQuickItem::heightChanged, item(), [this](){
        updateGeometry();
    });

    return move();
}

void ListViewSection::setOwner(ListViewItem* newParent)
{
    if (m_pOwner == newParent)
        return;

    if (m_pOwner->item()) {
        auto otherAnchors = qvariant_cast<QObject*>(newParent->item()->property("anchors"));
        auto anchors = qvariant_cast<QObject*>(m_pOwner->item()->property("anchors"));

        const auto newPrevious = static_cast<ListViewItem*>(m_pOwner->next(Qt::TopEdge));
        Q_ASSERT(newPrevious != m_pOwner);

        // Prevent a loop while moving
        if (otherAnchors && otherAnchors->property("top") == m_pOwner->item()->property("bottom")) {
            anchors->setProperty("top", {});
            otherAnchors->setProperty("top", {});
            otherAnchors->setProperty("y", {});
        }

        anchors->setProperty("top", newParent->item() ?
            newParent->item()->property("bottom") : QVariant()
        );

        // Set the old owner new anchors
        if (newPrevious && newPrevious->item())
            anchors->setProperty("top", newPrevious->item()->property("bottom"));

        otherAnchors->setProperty("top", m_pItem->property("bottom"));
    }
    else
        newParent->item()->setY(0);

    // Cleanup the previous owner decoration
    if (m_pOwner) {
        m_pOwner->setBorderDecoration(Qt::TopEdge, 0);
    }

    m_pOwner = newParent;

    // Update the owner decoration size
    m_pOwner->setBorderDecoration(Qt::TopEdge, 45.0);
}

void ListViewSection::reparentSection(ListViewItem* newParent, ViewBase* view)
{
    if (!m_pItem)
        return;

    if (newParent && newParent->item()) {
        auto anchors = qvariant_cast<QObject*>(m_pItem->property("anchors"));
        anchors->setProperty("top", newParent->item()->property("bottom"));

        m_pItem->setParentItem(owner()->view()->contentItem());
    }
    else {
        auto anchors = qvariant_cast<QObject*>(m_pItem->property("anchors"));
        anchors->setProperty("top", {});
        m_pItem->setY(0);
    }

    if (!m_pItem->width())
        m_pItem->setWidth(view->contentItem()->width());

    // Update the chain //FIXME crashes
    /*if (newParent && newParent->m_pSection && newParent->m_pSection->m_pNext != this) {
        Q_ASSERT(newParent->m_pSection != this);
        Q_ASSERT(newParent->m_pSection->m_pNext != this);
        Q_ASSERT(newParent->m_pSection->m_pNext->m_pPrevious != this);
        m_pNext = newParent->m_pSection->m_pNext;
        newParent->m_pSection->m_pNext = this;
        m_pPrevious = newParent->m_pSection;
        if (m_pNext) {
            m_pNext->m_pPrevious = this;
            Q_ASSERT(m_pNext != this);
        }
        Q_ASSERT(m_pPrevious != this);
    }
    else if (!newParent) {
        m_pPrevious = nullptr;
        m_pNext = d_ptr->m_pFirstSection;
        d_ptr->m_pFirstSection = this;
    }*/
}

bool ListViewItem::move()
{
    auto prev = static_cast<ListViewItem*>(next(Qt::TopEdge));

    const QQuickItem* prevItem = nullptr;

    if (d()->m_pSections)
        if (auto sec = d()->getSection(this)) {
            // The item is no longer the first in the section
            if (sec->owner() == this) {

                ListViewItem* newOwner = nullptr;
                while (prev && prev->m_pSection == sec) {
                    newOwner = prev;
                    prev = static_cast<ListViewItem*>(prev->next(Qt::TopEdge));
                }

                if (newOwner) {
                    if (newOwner == static_cast<ListViewItem*>(next(Qt::TopEdge)))
                        prev = newOwner;

                    sec->setOwner(newOwner);
                }
                sec->reparentSection(prev, view());

                if (sec->m_pItem)
                    prevItem = sec->m_pItem;

            }
            else if (sec->owner()->row() > row()) { //TODO remove once correctly implemented
                //HACK to force reparenting when the elements move up
                sec->setOwner(this);
                sec->reparentSection(prev, view());
                prevItem = sec->m_pItem;
            }
        }

    // Reset the "real" previous element
    prev = static_cast<ListViewItem*>(next(Qt::TopEdge));

    const qreal y = d()->m_DepthChart.first()*row();

    if (item()->width() != view()->contentItem()->width())
        item()->setWidth(view()->contentItem()->width());

    prevItem = prevItem ? prevItem : prev ? prev->item() : nullptr;

    // So other items can be GCed without always resetting to 0x0, note that it
    // might be a good idea to extend Flickable to support a virtual
    // origin point.
    if (!prevItem)
        item()->setY(y);
    else {
        // Row can be 0 if there is a section
        Q_ASSERT(row() || (!prev) || (!prev->item()));

        // Prevent loops when swapping 2 items
        auto otherAnchors = qvariant_cast<QObject*>(prevItem->property("anchors"));
        auto anchors = qvariant_cast<QObject*>(item()->property("anchors"));

        if (otherAnchors && otherAnchors->property("top") == item()->property("bottom")) {
            anchors->setProperty("top", {});
            otherAnchors->setProperty("top", {});
            otherAnchors->setProperty("y", {});
        }

        // Avoid creating too many race conditions
        if (anchors->property("top") != prevItem->property("bottom"))
            anchors->setProperty("top", prevItem->property("bottom"));

    }

    updateGeometry();

    return true;
}

bool ListViewItem::remove()
{
    if (m_pSection && --m_pSection->m_RefCount <= 0) {
        delete m_pSection;
    }
    else if (m_pSection && m_pSection->owner() == this) {
        // Reparent the section
        if (auto n = static_cast<ListViewItem*>(next(Qt::BottomEdge))) {
            if (n->m_pSection == m_pSection)
                m_pSection->setOwner(n);
            /*else
                Q_ASSERT(false);*/
        }
        /*else
            Q_ASSERT(false);*/
    }

    m_pSection = nullptr;

    //TODO move back into treeview2
    //TODO check if the item has references, if it does, just release the shared
    // pointer and move on.
    return true;
}

ListViewSections::ListViewSections(ListView* parent) :
    QObject(parent), d_ptr(parent->d_ptr)
{
}

ListViewSection::~ListViewSection()
{

    if (m_pPrevious) {
        Q_ASSERT(m_pPrevious != m_pNext);
        m_pPrevious->m_pNext = m_pNext;
    }

    if (m_pNext)
        m_pNext->m_pPrevious = m_pPrevious;

    if (this == d_ptr->m_pFirstSection)
        d_ptr->m_pFirstSection = m_pNext;

    d_ptr->m_IndexLoaded = false;

    if (m_pItem)
        delete m_pItem;

    if (m_pContent)
        delete m_pContent;
}

ListViewSections::~ListViewSections()
{

    delete d_ptr;
}

QQmlComponent* ListViewSections::delegate() const
{
    return d_ptr->m_pDelegate;
}

void ListViewSections::setDelegate(QQmlComponent* component)
{
    d_ptr->m_pDelegate = component;
}

QString ListViewSections::property() const
{
    return d_ptr->m_Property;
}

int ListViewSections::role() const
{
    if (d_ptr->m_Property.isEmpty() || !d_ptr->q_ptr->rawModel())
        return Qt::DisplayRole;

    if (d_ptr->m_CachedRole)
        return d_ptr->m_CachedRole;

    const auto roles = d_ptr->q_ptr->rawModel()->roleNames();

    if (!(d_ptr->m_CachedRole = roles.key(d_ptr->m_Property.toLatin1()))) {
        qWarning() << d_ptr->m_Property << "is not a model property";
        return Qt::DisplayRole;
    }

    return d_ptr->m_CachedRole;
}

void ListViewSections::setProperty(const QString& property)
{
    d_ptr->m_Property = property;
}

QStringList ListViewSections::roles() const
{
    return d_ptr->m_Roles;
}

void ListViewSections::setRoles(const QStringList& list)
{
    d_ptr->m_Roles = list;
}

QSharedPointer<QAbstractItemModel> ListViewSections::model() const
{
    return d_ptr->m_pSectionModel;
}

void ListViewSections::setModel(const QSharedPointer<QAbstractItemModel>& m)
{
    d_ptr->m_pSectionModel = m;
}

void ListViewPrivate::slotCurrentIndexChanged(const QModelIndex& index)
{
    emit q_ptr->indexChanged(index.row());
}

// Make sure the section stay in sync with the content
void ListViewPrivate::slotDataChanged(const QModelIndex& tl, const QModelIndex& br)
{
    if ((!m_pSections) || (!tl.isValid()) || (!br.isValid()))
        return;

    //FIXME remove this call to itemForIndex and delete the function
    auto tli = static_cast<ListViewItem*>(q_ptr->itemForIndex(tl));
    auto bri = static_cast<ListViewItem*>(q_ptr->itemForIndex(br));

//     Q_ASSERT(tli);
//     Q_ASSERT(bri);
//
//     bool outdated = false;
//
//     //TODO there is some possible optimizations here, not *all* subsequent
//     // elements needs to be moved
//     do {
//         if (outdated || (outdated = (tli->m_pSection != getSection(tli))))
//             tli->move();
//
//     } while(tli != bri && (tli = static_cast<ListViewItem*>(tli->down())));
}

ListViewItem *ListViewSection::owner() const
{
    return m_pOwner;
}

#include <listview.moc>
