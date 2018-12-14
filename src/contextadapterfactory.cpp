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
#include "contextadapterfactory.h"

// Qt
#include <QtCore/QAbstractItemModel>
#include <QtCore/QMutex>
#include <QtCore/private/qmetaobjectbuilder_p.h>
#include <QQmlContext>
#include <QQuickItem>
#include <QQmlEngine>

// KQuickItemViews
#include "adapters/abstractitemadapter.h"
#include "viewbase.h"
#include "viewport.h"
#include "private/viewport_p.h"
#include "private/indexmetadata_p.h"
#include "extensions/contextextension.h"
#include "adapters/modeladapter.h"
#include "private/statetracker/viewitem_p.h"
#include "adapters/contextadapter.h"

using FactoryFunctor = std::function<ContextAdapter*(QQmlContext*)>;

/**
 * Add some metadata to be able to use QAbstractItemModel roles as QObject
 * properties.
 */
struct MetaProperty
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
        IS_ROLE     = 0x1 << 7, /*!< When the MetaProperty map to a model role */
    };
    int flags {Flags::UNUSED};

    /// Q_PROPERTY internal id
    int propId;

    /// The role ID from QAbstractItemModel::roleNames
    int roleId {-1};

    /**
     * The name ID from QAbstractItemModel::roleNames
     *
     * (the pointer *is* on purpose reduce cache faults in this hot code path)
     */
    QByteArray* name {nullptr};

    uint signalId;
};

struct GroupMetaData
{
    ContextExtension* ptr;
    uint offset;
};

/**
 * Keep the offset and id of the extension for validation and change notification.
 */
class ContextExtensionPrivate
{
public:
    uint m_Id     {0};
    uint m_Offset {0};
    ContextAdapterFactoryPrivate *d_ptr {nullptr};
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
    explicit DynamicMetaType(const QHash<int, QByteArray>& roles);
    ~DynamicMetaType() {
        delete[] roles;//FIXME leak [roleCount*sizeof(MetaProperty))];
    }

    const size_t              roleCount     {   0   };
    size_t                    propertyCount {   0   };
    MetaProperty*             roles         {nullptr};
    QSet<MetaProperty*>       m_lUsed       {       };
    QMetaObject              *m_pMetaObject {nullptr};
    bool                      m_GroupInit   { false };
    QHash<int, MetaProperty*> m_hRoleIds    {       };
    uint8_t                  *m_pCacheMap   {nullptr};

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
    explicit DynamicContext(ContextAdapterFactory* mt);
    virtual ~DynamicContext();

    // Some "secret" QObject methods.
    virtual int qt_metacall(QMetaObject::Call call, int id, void **argv) override;
    virtual void* qt_metacast(const char *name) override;
    virtual const QMetaObject *metaObject() const override;

    // Use a C array to prevent the array bound checks
    QVariant              **m_lVariants {nullptr};
    DynamicMetaType       * m_pMetaType {nullptr};
    bool                    m_Cache     { true  };
    QQmlContext           * m_pCtx      {nullptr};
    QPersistentModelIndex   m_Index     {       };
    QQmlContext           * m_pParentCtx{nullptr};
    QMetaObject::Connection m_Conn;

    ContextAdapterFactoryPrivate* d_ptr {nullptr};
    ContextAdapter* m_pBuilder;
};

class ContextAdapterFactoryPrivate
{
public:
    // Attributes
    QList<ContextExtension*>  m_lGroups   {       };
    mutable DynamicMetaType  *m_pMetaType {nullptr};
    QAbstractItemModel       *m_pModel    {nullptr};

    FactoryFunctor m_fFactory;

    // Helper
    void initGroup(const QHash<int, QByteArray>& rls);
    void finish();

    ContextAdapterFactory* q_ptr;
};

/**
 * Create a group of virtual Q_PROPERTY to match the model role names.
 */
class RoleGroup final : public ContextExtension
{
public:
    explicit RoleGroup(ContextAdapterFactoryPrivate* d) : d_ptr(d) {}

    // The implementation isn't necessary in this case given it uses a second
    // layer of vTable instead of a static list. It goes against the
    // documentation, but that's on purpose.
    virtual QVariant getProperty(AbstractItemAdapter* item, uint id, const QModelIndex& index) const override;
    virtual uint size() const override;
    virtual QByteArray getPropertyName(uint id) const override;
    virtual bool setProperty(AbstractItemAdapter* item, uint id, const QVariant& value, const QModelIndex& index) const;


    ContextAdapterFactoryPrivate* d_ptr;
};

ContextAdapterFactory::ContextAdapterFactory(FactoryFunctor f) : d_ptr(new ContextAdapterFactoryPrivate())
{
    d_ptr->q_ptr = this;
    d_ptr->m_fFactory = f;

    addContextExtension(new RoleGroup(d_ptr));
}

ContextAdapterFactory::~ContextAdapterFactory()
{
    delete d_ptr;
}

uint ContextExtension::size() const
{
    return propertyNames().size();
}

bool ContextExtension::supportCaching(uint id) const
{
    Q_UNUSED(id)
    return true;
}

QVector<QByteArray>& ContextExtension::propertyNames() const
{
    static QVector<QByteArray> r;
    return r;
}

QByteArray ContextExtension::getPropertyName(uint id) const
{
    return propertyNames()[id];
}

bool ContextExtension::setProperty(AbstractItemAdapter* item, uint id, const QVariant& value, const QModelIndex& index) const
{
    Q_UNUSED(item)
    Q_UNUSED(id)
    Q_UNUSED(value)
    Q_UNUSED(index)
    return false;
}

void ContextExtension::changeProperty(AbstractItemAdapter* item, uint id)
{
    Q_UNUSED(item)
    Q_UNUSED(id)
    Q_ASSERT(false);
/*
    const auto metaRole = &d_ptr->d_ptr->m_pMetaType->roles[id];
    Q_ASSERT(metaRole);

    auto mo = d_ptr->d_ptr->m_pMetaType->m_pMetaObject;

    index()*/
}

void ContextAdapter::flushCache()
{
    for (uint i = 0; i < d_ptr->m_pMetaType->propertyCount; i++) {
        if (d_ptr->m_lVariants[i])
            delete d_ptr->m_lVariants[i];

        d_ptr->m_lVariants[i] = nullptr;
    }
}

void AbstractItemAdapter::dismissCacheEntry(ContextExtension* e, int id)
{
    auto dx = s_ptr->m_pMetadata->contextAdapter()->d_ptr;
    if (!dx)
        return;

    Q_ASSERT(e);
    Q_ASSERT(e->d_ptr->d_ptr->q_ptr == viewport()->modelAdapter()->contextAdapterFactory());
    Q_ASSERT(e->d_ptr->d_ptr->m_lGroups[e->d_ptr->m_Id] == e);
    Q_ASSERT(id >= 0 && id < (int) e->size());

    dx->m_lVariants[e->d_ptr->m_Offset + id] = nullptr;
}

QVariant RoleGroup::getProperty(AbstractItemAdapter* item, uint id, const QModelIndex& index) const
{
    Q_UNUSED(item)
    const auto metaRole = &d_ptr->m_pMetaType->roles[id];

    // Keep track of the accessed roles
    if (!(metaRole->flags & MetaProperty::Flags::READ))
        d_ptr->m_pMetaType->m_lUsed << metaRole;

    metaRole->flags |= MetaProperty::Flags::READ;

    return index.data(metaRole->roleId);
}

uint RoleGroup::size() const
{
    return d_ptr->m_pMetaType->roleCount;
}

QByteArray RoleGroup::getPropertyName(uint id) const
{
    return *d_ptr->m_pMetaType->roles[id].name;
}

bool RoleGroup::setProperty(AbstractItemAdapter* item, uint id, const QVariant& value, const QModelIndex& index) const
{
    // Avoid "useless" setData. With binding loops this can get very nasty.
    // Yes: You could to do this on purpose in the past, but it's no longer
    // safe.
    if (getProperty(item, id, index) == value)
        return false;

    const auto metaRole = &d_ptr->m_pMetaType->roles[id];

    // Keep track of the accessed roles
    if (!(metaRole->flags & MetaProperty::Flags::TRIED_WRITE))
        d_ptr->m_pMetaType->m_lUsed << metaRole;

    metaRole->flags |= MetaProperty::Flags::TRIED_WRITE;

    if (d_ptr->m_pModel->setData(index, value, metaRole->roleId)) {
        metaRole->flags |= MetaProperty::Flags::HAS_WRITTEN;
        return true;
    }

    return false;
}

const QMetaObject *DynamicContext::metaObject() const
{
    Q_ASSERT(m_pMetaType);
    return m_pMetaType->m_pMetaObject;
}

int DynamicContext::qt_metacall(QMetaObject::Call call, int id, void **argv)
{
    const int realId = id - m_pMetaType->m_pMetaObject->propertyOffset();

    //qDebug() << "META" << id << realId << call << QMetaObject::ReadProperty;
    if (realId < 0)
        return QObject::qt_metacall(call, id, argv);

    const auto group = &m_pMetaType->m_lGroupMapping[realId];
    Q_ASSERT(group->ptr);

    if (call == QMetaObject::ReadProperty) {
        if (Q_UNLIKELY(((size_t)realId) >= m_pMetaType->propertyCount)) {
            Q_ASSERT(false);
            return -1;
        }

        const QModelIndex idx = m_pBuilder->item() ? m_pBuilder->item()->index() : m_Index;

        const bool supportsCache = m_Cache &&
            (m_pMetaType->m_pCacheMap[realId/8] & (1 << (realId % 8)));

        // Use a special function for the role case. It's only known at runtime.
        QVariant *value = m_lVariants[realId] && supportsCache
            ? m_lVariants[realId] : new QVariant(
                group->ptr->getProperty(m_pBuilder->item(), realId - group->offset, idx));

        if (supportsCache && !m_lVariants[realId])
            m_lVariants[realId] = value;

        QMetaType::construct(QMetaType::QVariant, argv[0], value->data());

//         if (!supportsCache)
//             delete value;
    }
    else if (call == QMetaObject::WriteProperty) {
        const QVariant  value = QVariant(QMetaType::QVariant, argv[0]).value<QVariant>();
        const QModelIndex idx = m_pBuilder->item() ? m_pBuilder->item()->index() : m_Index;

        const bool ret = group->ptr->setProperty(
            m_pBuilder->item(), realId - group->offset, value, idx
        );

        // Register if setting the property worked
        *reinterpret_cast<int*>(argv[2]) = ret ? 1 : 0;

        QMetaObject::activate(this, m_pMetaType->m_pMetaObject, realId, argv);

    }
    else if (call == QMetaObject::InvokeMetaMethod) {
        int sigId = id - m_pMetaType->m_pMetaObject->methodOffset();
        qDebug() << "LA LA INVOKE" << sigId << id;
        QMetaObject::activate(this,  m_pMetaType->m_pMetaObject, id, nullptr);
        return -1;
    }

    return -1;
}

void* DynamicContext::qt_metacast(const char *name)
{
    if (!strcmp(name, m_pMetaType->m_pMetaObject->className()))
        return this;

    return QObject::qt_metacast(name);
}

DynamicMetaType::DynamicMetaType(const QHash<int, QByteArray>& rls) :
roleCount(rls.size())
{}

/// Populate a vTable with the propertyId -> group object
void ContextAdapterFactoryPrivate::initGroup(const QHash<int, QByteArray>& rls)
{
    Q_ASSERT(!m_pMetaType->m_GroupInit);

    for (auto group : qAsConst(m_lGroups))
        m_pMetaType->propertyCount += group->size();

    m_pMetaType->m_lGroupMapping = (GroupMetaData*) malloc(
        sizeof(GroupMetaData) * m_pMetaType->propertyCount
    );

    uint offset(0), realId(0), groupId(0);

    for (auto group : qAsConst(m_lGroups)) {
        Q_ASSERT(!group->d_ptr->d_ptr);
        group->d_ptr->d_ptr    = this;
        group->d_ptr->m_Offset = offset;
        group->d_ptr->m_Id     = groupId++;

        const uint gs = group->size();

        for (uint i = 0; i < gs; i++)
            m_pMetaType->m_lGroupMapping[offset+i] = {group, offset};

        offset += gs;
    }
    Q_ASSERT(offset == m_pMetaType->propertyCount);

    // Add a bitfield to store the properties that need to skip the cache
    const int fieldSize = m_pMetaType->propertyCount / 8 + (m_pMetaType->propertyCount%8?1:0);
    m_pMetaType->m_pCacheMap = (uint8_t*) malloc(fieldSize);
    for (int i = 0; i < fieldSize; i++)
        m_pMetaType->m_pCacheMap[i] = 0;

    m_pMetaType->m_GroupInit = true;

    // Create the metaobject
    QMetaObjectBuilder builder;
    builder.setClassName("DynamicContext");
    builder.setSuperClass(&QObject::staticMetaObject);

    // Use a C array like the moc would do because this is called **A LOT**
    m_pMetaType->roles = new MetaProperty[m_pMetaType->propertyCount];

    // Setup the role metadata
    for (auto i = rls.constBegin(); i != rls.constEnd(); i++) {
        uint id = realId++;
        MetaProperty* r = &m_pMetaType->roles[id];

        r->roleId = i.key();
        r->name   = new QByteArray(i.value());
        r->flags |= MetaProperty::Flags::IS_ROLE;

        m_pMetaType->m_hRoleIds[i.key()] = r;
    }

    realId = 0;

    // Add all object virtual properties
    for (const auto g : qAsConst(m_lGroups)) {
        for (uint j = 0; j < g->size(); j++) {
            uint id = realId++;
            Q_ASSERT(id < m_pMetaType->propertyCount);

            MetaProperty* r = &m_pMetaType->roles[id];
            r->propId   = id;
            const auto name = g->getPropertyName(j);

            auto property = builder.addProperty(name, "QVariant");
            property.setWritable(true);

            auto signal = builder.addSignal(name + "Changed()");
            r->signalId = signal.index();
            property.setNotifySignal(signal);

            // Set the cache bit
            m_pMetaType->m_pCacheMap[id/8] |= (g->supportCaching(j)?1:0) << (id % 8);
        }
    }

    m_pMetaType->m_pMetaObject = builder.toMetaObject();
}

DynamicContext::DynamicContext(ContextAdapterFactory* cm) :
    m_pMetaType(cm->d_ptr->m_pMetaType)
{
    Q_ASSERT(m_pMetaType);
    Q_ASSERT(m_pMetaType->roleCount <= m_pMetaType->propertyCount);

    m_lVariants = (QVariant**) malloc(sizeof(QVariant*)*m_pMetaType->propertyCount);

    //TODO SIMD this
    for (uint i = 0; i < m_pMetaType->propertyCount; i++)
        m_lVariants[i] = nullptr;
}

DynamicContext::~DynamicContext()
{}

//FIXME delete the metatype now that it's invalid.
//     if (m_pMetaType) {
//         qDeleteAll(m_hContextMapper);
//         m_hContextMapper.clear();
//
//         delete m_pMetaType;
//         m_pMetaType = nullptr;
//     }

bool ContextAdapter::updateRoles(const QVector<int> &modified) const
{
    if (!d_ptr->d_ptr->m_pMetaType)
        return false;

    bool ret = false;

    if (!modified.isEmpty()) {
        for (auto r : qAsConst(modified)) {
            if (auto mr = d_ptr->d_ptr->m_pMetaType->m_hRoleIds.value(r)) {
                // This works because the role offset is always 0
                if (d_ptr->m_lVariants[mr->propId]) {
                    delete d_ptr->m_lVariants[mr->propId];
                    d_ptr->m_lVariants[mr->propId] = nullptr;
                    QMetaMethod m = d_ptr->metaObject()->method(mr->signalId);
                    m.invoke(d_ptr);
                    ret |= ret;
                }
            }
        }
    }
    else {
        // Only update the roles known to have an impact
        for (auto mr : qAsConst(d_ptr->d_ptr->m_pMetaType->m_lUsed)) {
            // Use `READ` instead of checking the cache because it could have
            // been dismissed for many reasons.
            if ((!d_ptr->m_Cache) || mr->flags & MetaProperty::Flags::READ) {
                if (auto v = d_ptr->m_lVariants[mr->propId])
                    delete v;

                d_ptr->m_lVariants[mr->propId] = nullptr;

                //FIXME this should work, but it doesn't
                auto mo = d_ptr->d_ptr->m_pMetaType->m_pMetaObject;
                const int methodId = mo->methodOffset() + mr->signalId;
                QMetaMethod m = mo->method(methodId);
                //m.invoke(d_ptr);
                Q_ASSERT(m.name() == (*mr->name)+"Changed");

                //FIXME this should also work, but also doesn't
                //QMetaObject::activate(d_ptr, mo, mr->signalId, nullptr);

                //FIXME Use this for now, but it prevent setData from being implemented
                d_ptr->setProperty(*mr->name, d_ptr->property(*mr->name));

                QMetaObject::activate(d_ptr, mo, mr->signalId, nullptr);

                ret = true;
            }
        }
    }

    return ret;
}

QAbstractItemModel *ContextAdapterFactory::model() const
{
    return d_ptr->m_pModel;
}

void ContextAdapterFactory::setModel(QAbstractItemModel *m)
{
    d_ptr->m_pModel = m;
}

void ContextAdapterFactoryPrivate::finish()
{
    Q_ASSERT(m_pModel);

    if (m_pMetaType)
        return;

    const auto roles = m_pModel->roleNames();

    m_pMetaType = new DynamicMetaType(roles);
    initGroup(roles);
}

void ContextAdapterFactory::addContextExtension(ContextExtension* pg)
{
    Q_ASSERT(!d_ptr->m_pMetaType);

    if (d_ptr->m_pMetaType) {
        qWarning() << "It is not possible to add property group after creating a builder";
        return;
    }

    d_ptr->m_lGroups << pg;
}

QSet<QByteArray> ContextAdapterFactory::usedRoles() const
{
    if (!d_ptr->m_pMetaType)
        return {};

    QSet<QByteArray> ret;

    for (const auto mr : qAsConst(d_ptr->m_pMetaType->m_lUsed)) {
        if (mr->roleId != -1)
            ret << *mr->name;
    }

    return ret;
}

ContextAdapter*
ContextAdapterFactory::createAdapter(FactoryFunctor f, QQmlContext *parentContext) const
{
    ContextAdapter* ret = f(parentContext);

    Q_ASSERT(!ret->d_ptr);

    d_ptr->finish();
    ret->d_ptr = new DynamicContext(const_cast<ContextAdapterFactory*>(this));
    ret->d_ptr->d_ptr = d_ptr;
    ret->d_ptr->m_pBuilder   = ret;
    ret->d_ptr->m_pParentCtx = parentContext;

    // Use the core application because the parentContext have an unpredictable
    // lifecycle and the object may be reparented anyway. If it has no parent,
    // QtQuick can take ownership and will also destroy it.
    ret->d_ptr->setParent(QCoreApplication::instance());

    //HACK QtQuick ignores
    ret->d_ptr->m_Conn = QObject::connect(ret->d_ptr, &QObject::destroyed, ret->d_ptr, [ret, this, parentContext]() {
        qWarning() << "Rebuilding the cache because QtQuick bugs trashed it";
        ret->d_ptr = new DynamicContext(const_cast<ContextAdapterFactory*>(this));
        ret->d_ptr->d_ptr = d_ptr;
        ret->d_ptr->setParent(QCoreApplication::instance());
        ret->d_ptr->m_pBuilder   = ret;
        ret->d_ptr->m_pParentCtx = parentContext;
    });

    return ret;
}

ContextAdapter* ContextAdapterFactory::createAdapter(QQmlContext *parentContext) const
{
    return createAdapter(d_ptr->m_fFactory, parentContext);
}

ContextAdapter::ContextAdapter(QQmlContext *parentContext)
{
    Q_UNUSED(parentContext)
}

ContextAdapter::~ContextAdapter()
{
    if (d_ptr->m_pCtx)
        d_ptr->m_pCtx->setContextObject(nullptr);

    QObject::disconnect(d_ptr->m_Conn);

    d_ptr->m_pBuilder = nullptr;

    delete d_ptr;
}

bool ContextAdapter::isCacheEnabled() const
{
    return d_ptr->m_Cache;
}

void ContextAdapter::setCacheEnabled(bool v)
{
    d_ptr->m_Cache = v;
}

QModelIndex ContextAdapter::index() const
{
    return d_ptr->m_Index;
}

void ContextAdapter::setModelIndex(const QModelIndex& index)
{
    const bool hasIndex = d_ptr->m_Index.isValid();

    if (d_ptr->m_Cache)
        flushCache();

    d_ptr->m_Index = index;

    if (hasIndex)
        updateRoles({});
}

QQmlContext* ContextAdapter::context() const
{
    Q_ASSERT(d_ptr);

    if (!d_ptr->m_pCtx) {
        d_ptr->m_pCtx = new QQmlContext(d_ptr->m_pParentCtx, d_ptr->parent());
        d_ptr->m_pCtx->setContextObject(d_ptr);
        d_ptr->m_pCtx->engine()->setObjectOwnership(
            d_ptr, QQmlEngine::CppOwnership
        );
        d_ptr->m_pCtx->engine()->setObjectOwnership(
            d_ptr->m_pCtx, QQmlEngine::CppOwnership
        );
    }

    return d_ptr->m_pCtx;
}

QObject *ContextAdapter::contextObject() const
{
    return d_ptr;
}

bool ContextAdapter::isActive() const
{
    return d_ptr->m_pCtx;
}

AbstractItemAdapter* ContextAdapter::item() const
{
    return nullptr;
}

ContextExtension::ContextExtension() : d_ptr(new ContextExtensionPrivate())
{}

