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

// Qt
#include <QtCore/QAbstractItemModel>
#include <QtCore/private/qmetaobjectbuilder_p.h>
#include <QQmlContext>
#include <QQuickItem>

#include "abstractviewitem.h"
#include "abstractquickview.h"
#include "abstractviewitem_p.h"

/**
 * Add some metadata to be able to use QAbstractItemModel roles as QObject
 * properties.
 */
struct MetaRole
{
    // Bitmask to hold (future) metadata regarding how the property is used
    // by QML.
    enum Flags : int {
        UNUSED      = 0x0 << 0, /*!< This property was never used          */
        READ        = 0x1 << 0, /*!< If data() was ever called             */
        HAS_DATA    = 0x1 << 1, /*!< When the QVariant is valid            */
        TRIED_WRITE = 0x1 << 2, /*!< When setData returns false            */
        HAS_WRITTEN = 0x1 << 3, /*!< When setData returns true             */
        HAS_CHANGED = 0x1 << 4, /*!< When the value was queried many times */
        HAS_SUBSET  = 0x1 << 5, /*!< When dataChanged has a role list      */
        HAS_GLOBAL  = 0x1 << 6, /*!< When dataChanged has no role          */
    };
    int flags {Flags::UNUSED};

    /// Q_PROPERTY internal id
    int propId;

    /// The role ID from QAbstractItemModel::roleNames
    int roleId;

    /**
     * The name ID from QAbstractItemModel::roleNames
     *
     * (the pointer *is* on purpose reduce cache faults in this hot code path)
     */
    QByteArray* name;
};

struct GroupMetaData
{
    ContextManager::PropertyGroup* ptr;
    int offset;
};

/**
 * This struct is the internal representation normally built by the Qt MOC
 * generator.
 *
 * It holds a "fake" type of QObject designed to reflect the model roles as
 * QObject properties. It also tracks the property being *used* by QML to
 * prevent too many events being pushed into the QML context.
 */
struct DynamicMetaType final
{
    explicit DynamicMetaType(const QByteArray& className, const QHash<int, QByteArray>& roles);
    ~DynamicMetaType() {
        delete[] roles;//FIXME leak [roleCount*sizeof(MetaRole))];
    }

    const size_t          roleCount      {   0   };
    size_t                propertyCount  {   0   };
    MetaRole*             roles          {nullptr};
    QSet<MetaRole*>       used           {       };
    QMetaObject           *m_pMetaObject {nullptr};
    bool                  m_GroupInit    { false };
    QHash<int, MetaRole*> m_hRoleIds     {       };

    /**
     * Assuming the number of role is never *that* high, keep a jump map to
     * prevent doing dispatch vTable when checking the properties source.
     *
     * In theory it can be changed at runtime if the need arise, but for now
     * its static. Better harden the binaries a bit, having call maps on the
     * heap isn't the most secure scheme in the universe.
     */
    GroupMetaData* m_lGroupMapping {nullptr};
};

class DynamicContext final : public QObject
{
public:
    explicit DynamicContext(DynamicMetaType* mt, AbstractViewItem* item);
    virtual ~DynamicContext();

    // Some "secret" QObject methods.
    virtual int qt_metacall(QMetaObject::Call call, int id, void **argv) override;
    virtual void* qt_metacast(const char *name) override;
    virtual const QMetaObject *metaObject() const override;

    // Use a C array to prevent the array bound checks
    QVariant         **m_lVariants {nullptr};
    DynamicMetaType  *m_pMetaType  {nullptr};
    AbstractViewItem *m_pItem      {nullptr};

    ContextManagerPrivate* d_ptr;
};

class ContextManagerPrivate
{
public:
    QList<ContextManager::PropertyGroup*> m_lGroups;

    void initGroup(const QHash<int, QByteArray>& rls);
    mutable DynamicMetaType *m_pMetaType {nullptr};

    ContextManager* q_ptr;
};

/**
 * Create a group of virtual Q_PROPERTY to match the model role names.
 */
class RoleGroup final : public ContextManager::PropertyGroup
{
public:
    explicit RoleGroup(ContextManagerPrivate* d) : d_ptr(d) {}

    // The implementation isn't necessary in this case given it uses a second
    // layer of vTable instead of a static list. It goes against the
    // documentation, but that's on purpose.
    virtual QVariant getProperty(AbstractViewItem* item, uint id) const override;
    virtual uint size() const override;
    virtual QByteArray getPropertyName(uint id) override;

    ContextManagerPrivate* d_ptr;
};

ContextManager::ContextManager() : d_ptr(new ContextManagerPrivate())
{
    d_ptr->q_ptr = this;
    addPropertyGroup(new RoleGroup(d_ptr));
}

ContextManager::~ContextManager()
{
    delete d_ptr;
}

uint ContextManager::PropertyGroup::size() const
{
    return propertyNames().size();
}
QVector<QByteArray>& ContextManager::PropertyGroup::propertyNames() const
{
    static QVector<QByteArray> r;
    return r;
}

QByteArray ContextManager::PropertyGroup::getPropertyName(uint id)
{
    return propertyNames()[id];
}

void ContextManager::PropertyGroup::setProperty(AbstractViewItem* item, uint id, const QVariant& value) const
{}

void ContextManager::PropertyGroup::changeProperty(AbstractViewItem* item, uint id)
{
    /*auto dx = item->s_ptr->dynamicContext();
    if (!dx)
        return;TODO*/
}

void AbstractViewItem::flushCache()
{
    auto dx = s_ptr->dynamicContext();
    if (!dx)
        return;

    for (int i = 0; i < dx->m_pMetaType->propertyCount; i++) {
        if (dx->m_lVariants[i])
            delete dx->m_lVariants[i];

        dx->m_lVariants[i] = nullptr;
    }
}

QVariant RoleGroup::getProperty(AbstractViewItem* item, uint id) const
{
    const auto metaRole = &d_ptr->m_pMetaType->roles[id];

    // Keep track of the accessed roles
    if (!(metaRole->flags & MetaRole::Flags::READ)) {
        d_ptr->m_pMetaType->used << metaRole;
        qDebug() << "\n NEW ROLE!" << (*metaRole->name) << d_ptr->m_pMetaType->used.size();
    }

    metaRole->flags |= MetaRole::Flags::READ;

    return item->index().data(metaRole->roleId);
}

uint RoleGroup::size() const
{
    return d_ptr->m_pMetaType->roleCount;
}

QByteArray RoleGroup::getPropertyName(uint id)
{
    return *d_ptr->m_pMetaType->roles[id].name;
}

const QMetaObject *DynamicContext::metaObject() const
{
    Q_ASSERT(m_pMetaType);
    return m_pMetaType->m_pMetaObject;
}

int DynamicContext::qt_metacall(QMetaObject::Call call, int id, void **argv)
{
    const int realId = id - m_pMetaType->m_pMetaObject->propertyOffset();

    if (realId < 0) {
        return QObject::qt_metacall(call, id, argv);
    }

    if (call == QMetaObject::ReadProperty) {
        if (Q_UNLIKELY(realId >= m_pMetaType->propertyCount)) {
            Q_ASSERT(false);
            return -1;
        }

        const auto group = &m_pMetaType->m_lGroupMapping[realId];
        Q_ASSERT(group->ptr);

        // Use a special function for the role case. It's only known at runtime.
        const QVariant& value = *(m_lVariants[realId]
            ? m_lVariants[realId] : m_lVariants[realId] = new QVariant(
                group->ptr->getProperty(m_pItem, realId - group->offset))
        );

        QMetaType::construct(QMetaType::QVariant, argv[0], value.data());
    }
    else if (call == QMetaObject::WriteProperty) {
        Q_ASSERT(false); //TODO call setData
        /*const int roleId = m_hIdMapper.value(realId);
        m_Index.model()->setData(
            m_Index, roleId, QVariant(property.typeId, argv[0])
        );
        m_lUsedProperties << roleId;
        *reinterpret_cast<int*>(argv[2]) = 1;  // setProperty return value
        QMetaObject::activate(this, m_pMetaObject, realId, nullptr);*/
    }

    return -1;
}

void* DynamicContext::qt_metacast(const char *name)
{
    if (!strcmp(name, m_pMetaType->m_pMetaObject->className()))
        return this;

    return QObject::qt_metacast(name);
}

DynamicMetaType::DynamicMetaType(const QByteArray& className, const QHash<int, QByteArray>& rls) :
roleCount(rls.size())
{}


/// Populate a vTable with the propertyId -> group object
void ContextManagerPrivate::initGroup(const QHash<int, QByteArray>& rls)
{
    Q_ASSERT(!m_pMetaType->m_GroupInit);

    for (auto group : qAsConst(m_lGroups))
        m_pMetaType->propertyCount += group->size();

    m_pMetaType->m_lGroupMapping = (GroupMetaData*) malloc(
        sizeof(GroupMetaData) * m_pMetaType->propertyCount
    );

    int offset(0), realId(0);

    for (auto group : qAsConst(m_lGroups)) {
        const int gs = group->size();

        for (int i = 0; i < gs; i++)
            m_pMetaType->m_lGroupMapping[offset+i] = {group, offset};

        offset += gs;
    }
    Q_ASSERT(offset == m_pMetaType->propertyCount);

    m_pMetaType->m_GroupInit = true;

    // Create the metaobject
    QMetaObjectBuilder builder;
    builder.setClassName("DynamicContext");
    builder.setSuperClass(&QObject::staticMetaObject);

    // Use a C array like the moc would do because this is called **A LOT**
    m_pMetaType->roles = new MetaRole[m_pMetaType->roleCount];

    // Setup the role metadata
    for (auto i = rls.constBegin(); i != rls.constEnd(); i++) {
        uint id = realId++;
        MetaRole* r = &m_pMetaType->roles[id];

        r->propId = id;
        r->roleId = i.key();
        r->name   = new QByteArray(i.value());

        m_pMetaType->m_hRoleIds[i.key()] = r;
    }

    // Add all object virtual properties
    for (const auto g : qAsConst(m_lGroups)) {
        for (int j = 0; j < g->size(); j++) {
            const auto name = g->getPropertyName(j);
            auto property = builder.addProperty(name, "QVariant");
            property.setWritable(true);

            auto signal = builder.addSignal(name + "Changed()");
            property.setNotifySignal(signal);
        }
    }

    m_pMetaType->m_pMetaObject = builder.toMetaObject();
}

DynamicContext::DynamicContext(DynamicMetaType* mt, AbstractViewItem* item) :
    m_pMetaType(mt), m_pItem(item)
{
    Q_ASSERT(m_pMetaType->roleCount >= 0 && m_pMetaType->roleCount <= m_pMetaType->propertyCount);

    m_lVariants = (QVariant**) malloc(sizeof(QVariant*)*m_pMetaType->propertyCount);

    //TODO SIMD this
    for (int i = 0; i < m_pMetaType->propertyCount; i++)
        m_lVariants[i] = nullptr;
}

DynamicContext::~DynamicContext()
{
    m_pItem->flushCache();
    m_pItem->context()->setContextObject(nullptr);
}

//FIXME delete the metatype now that it's invalid.
//     if (m_pMetaType) {
//         qDeleteAll(m_hContextMapper);
//         m_hContextMapper.clear();
//
//         delete m_pMetaType;
//         m_pMetaType = nullptr;
//     }

void AbstractViewItem::updateRoles(const QVector<int> &modified) const
{
    auto dx = s_ptr->dynamicContext();
    if (dx)
        return;

    for (auto r : qAsConst(modified)) {
        if (auto mr = dx->d_ptr->m_pMetaType->m_hRoleIds.value(r)) {
            dx->metaObject()->invokeMethod(
                dx, (*mr->name)+"Changed"
            );

            // This works because the role offset is always 0
            if (dx->m_lVariants[mr->propId])
                delete dx->m_lVariants[mr->propId];

            dx->m_lVariants[mr->propId] = nullptr;
        }
    }
}

void VisualTreeItem::updateContext()
{
    if (m_pDCtx)
        return;

    ContextManager* cm = d_ptr->view()->contextManager();
    const QModelIndex self = d_ptr->index();
    Q_ASSERT(self.model());

    const auto roles = self.model()->roleNames();

    if (!cm->d_ptr->m_pMetaType) {
        cm->d_ptr->m_pMetaType = new DynamicMetaType("DynamicModelContext", roles);
        cm->d_ptr->initGroup(roles);
    }

    d_ptr->context()->setContextObject(
        m_pDCtx = new DynamicContext(cm->d_ptr->m_pMetaType, d_ptr)
    );
}

void ContextManager::addPropertyGroup(ContextManager::PropertyGroup* pg)
{
    Q_ASSERT(!d_ptr->m_pMetaType);
    d_ptr->m_lGroups << pg;
}

DynamicContext* VisualTreeItem::dynamicContext() const
{
    return m_pDCtx;
}
