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
#include "flickable.h"

// Qt
#include <QtCore/QAbstractItemModel>
#include <QtCore/QEasingCurve>
#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlContext>

// LibStdC++
#include <cmath>

class FlickablePrivate final : public QObject
{
    Q_OBJECT
public:
    typedef bool(FlickablePrivate::*StateF)(QMouseEvent*);

    /// The current status of the inertial viewport state machine
    enum class DragState {
        IDLE    , /*!< Nothing is happening */
        PRESSED , /*!< A potential drag     */
        EVAL    , /*!< Drag without lock    */
        DRAGGED , /*!< An in progress grag  */
        INERTIA , /*!< A leftover drag      */
    };

    /// Events affecting the behavior of the inertial viewport state machine
    enum class DragEvent {
        TIMEOUT , /*!< When inertia is exhausted       */
        PRESS   , /*!< When a mouse button is pressed  */
        RELEASE , /*!< When a mouse button is released */
        MOVE    , /*!< When the mouse moves            */
        TIMER   , /*!< 30 times per seconds            */
        OTHER   , /*!< Doesn't affect the state        */
        ACCEPT  , /*!< Accept the drag ownership       */
        REJECT  , /*!< Reject the drag ownership       */
    };

    // Actions
    bool nothing (QMouseEvent* e); /*!< No operations                */
    bool error   (QMouseEvent* e); /*!< Warn something went wrong    */
    bool start   (QMouseEvent* e); /*!< From idle to pre-drag        */
    bool stop    (QMouseEvent* e); /*!< Stop inertia                 */
    bool drag    (QMouseEvent* e); /*!< Move the viewport            */
    bool cancel  (QMouseEvent* e); /*!< Cancel potential drag        */
    bool inertia (QMouseEvent* e); /*!< Iterate on the inertia curve */
    bool release (QMouseEvent* e); /*!< Trigger the inertia          */
    bool eval    (QMouseEvent* e); /*!< Check for potential drag ops */
    bool lock    (QMouseEvent* e); /*!< Lock the input grabber       */

    // Attributes
    QQuickItem* m_pContainer {nullptr};
    QPointF     m_StartPoint {       };
    QPointF     m_DragPoint  {       };
    QTimer*     m_pTimer     {nullptr};
    qint64      m_StartTime  {   0   };
    int         m_LastDelta  {   0   };
    qreal       m_Velocity   {   0   };
    qreal       m_DecelRate  {  0.9  };
    bool        m_Interactive{ true  };

    mutable QQmlContext *m_pRootContext {nullptr};

    qreal m_MaxVelocity {std::numeric_limits<qreal>::max()};

    DragState m_State {DragState::IDLE};

    // Helpers
    void loadVisibleElements();
    bool applyEvent(DragEvent event, QMouseEvent* e);
    bool updateVelocity();
    DragEvent eventMapper(QEvent* e) const;

    // State machine
    static const StateF m_fStateMachine[5][8];
    static const DragState m_fStateMap [5][8];

    Flickable* q_ptr;

public Q_SLOTS:
    void tick();
};

#define A &FlickablePrivate::           // Actions
#define S FlickablePrivate::DragState:: // Next state
/**
 * This is a Mealy machine, states callbacks are allowed to throw more events
 */
const FlickablePrivate::DragState FlickablePrivate::m_fStateMap[5][8] {
/*             TIMEOUT      PRESS      RELEASE     MOVE       TIMER      OTHER      ACCEPT    REJECT  */
/* IDLE    */ {S IDLE   , S PRESSED, S IDLE    , S IDLE   , S IDLE   , S IDLE   , S IDLE   , S IDLE   },
/* PRESSED */ {S PRESSED, S PRESSED, S IDLE    , S EVAL   , S PRESSED, S PRESSED, S PRESSED, S PRESSED},
/* EVAL    */ {S IDLE   , S EVAL   , S IDLE    , S EVAL   , S EVAL   , S EVAL   , S DRAGGED, S IDLE   },
/* DRAGGED */ {S DRAGGED, S DRAGGED, S INERTIA , S DRAGGED, S DRAGGED, S DRAGGED, S DRAGGED, S DRAGGED},
/* INERTIA */ {S IDLE   , S IDLE   , S IDLE    , S DRAGGED, S INERTIA, S INERTIA, S INERTIA, S INERTIA}};
const FlickablePrivate::StateF FlickablePrivate::m_fStateMachine[5][8] {
/*             TIMEOUT      PRESS      RELEASE     MOVE       TIMER      OTHER     ACCEPT   REJECT  */
/* IDLE    */ {A error  , A start  , A nothing , A nothing, A error  , A nothing, A error, A error  },
/* PRESSED */ {A error  , A nothing, A cancel  , A eval   , A error  , A nothing, A error, A error  },
/* EVAL    */ {A error  , A nothing, A cancel  , A eval   , A error  , A nothing, A lock , A cancel },
/* DRAGGED */ {A error  , A drag   , A release , A drag   , A error  , A nothing, A error, A error  },
/* INERTIA */ {A stop   , A stop   , A stop    , A error  , A inertia, A nothing, A error, A error  }};
#undef S
#undef A

Flickable::Flickable(QQuickItem* parent)
    : QQuickItem(parent), d_ptr(new FlickablePrivate)
{
    d_ptr->q_ptr = this;
    setClip(true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setFiltersChildMouseEvents(true);

    d_ptr->m_pTimer = new QTimer(this);
    d_ptr->m_pTimer->setInterval(1000/30);
    connect(d_ptr->m_pTimer, &QTimer::timeout, d_ptr, &FlickablePrivate::tick);
}

Flickable::~Flickable()
{
    if (d_ptr->m_pContainer)
        delete d_ptr->m_pContainer;
    delete d_ptr;
}

QQuickItem* Flickable::contentItem()
{
    if (!d_ptr->m_pContainer) {
        QQmlEngine *engine = QQmlEngine::contextForObject(this)->engine();

        // Can't be called too early as the engine wont be ready.
        Q_ASSERT(engine);

        QQmlComponent rect1(engine, this);
        rect1.setData("import QtQuick 2.4; Item {}", {});
        d_ptr->m_pContainer = qobject_cast<QQuickItem *>(rect1.create());
        d_ptr->m_pContainer->setHeight(height());
        d_ptr->m_pContainer->setWidth(width ());
        engine->setObjectOwnership(d_ptr->m_pContainer, QQmlEngine::CppOwnership);
        d_ptr->m_pContainer->setParentItem(this);

        emit contentHeightChanged(height());
    }

    return d_ptr->m_pContainer;
}

QRectF Flickable::viewport() const
{
    return {
        0.0,
        contentY(),
        width(),
        height()
    };
}

qreal Flickable::contentY() const
{
    if (!d_ptr->m_pContainer)
        return 0;

    return -d_ptr->m_pContainer->y();
}

void Flickable::setContentY(qreal y)
{
    if (!d_ptr->m_pContainer)
        return;

    // Do not allow out of bound scroll
    y = std::fmax(y, 0);

    if (d_ptr->m_pContainer->height() >= height())
        y = std::fmin(y, d_ptr->m_pContainer->height() - height());

    if (d_ptr->m_pContainer->y() == -y)
        return;

    d_ptr->m_pContainer->setY(-y);

    emit contentYChanged(y);
    emit viewportChanged(viewport());
    emit percentageChanged(
        ((-d_ptr->m_pContainer->y()))/(d_ptr->m_pContainer->height()-height())
    );
}

qreal Flickable::contentHeight() const
{
    if (!d_ptr->m_pContainer)
        return 0;

    return  d_ptr->m_pContainer->height();
}

/// Timer events
void FlickablePrivate::tick()
{
    applyEvent(DragEvent::TIMER, nullptr);
}

/**
 * Use the linear velocity. This class currently mostly ignore horizontal
 * movements, but nevertheless the intention is to keep the inertia factor
 * from its vector.
 *
 * @return If there is inertia
 */
bool FlickablePrivate::updateVelocity()
{
    const qreal dy = m_DragPoint.y() - m_StartPoint.y();
    const qreal dt = (QDateTime::currentMSecsSinceEpoch() - m_StartTime)/(1000.0/30.0);

    // Points per frame
    m_Velocity = (dy/dt);

    // Do not start for low velocity mouse release
    if (std::fabs(m_Velocity) < 40) //TODO C++17 use std::clamp
        m_Velocity = 0;

    if (std::fabs(m_Velocity) > std::fabs(m_MaxVelocity))
        m_Velocity = m_Velocity > 0 ? m_MaxVelocity : -m_MaxVelocity;

    return m_Velocity;
}

/**
 * Map qevent to DragEvent
 */
FlickablePrivate::DragEvent FlickablePrivate::eventMapper(QEvent* event) const
{
    auto e = FlickablePrivate::DragEvent::OTHER;

    #pragma GCC diagnostic ignored "-Wswitch-enum"
    switch(event->type()) {
        case QEvent::MouseMove:
            e = FlickablePrivate::DragEvent::MOVE;
            break;
        case QEvent::MouseButtonPress:
            e = FlickablePrivate::DragEvent::PRESS;
            break;
        case QEvent::MouseButtonRelease:
            e = FlickablePrivate::DragEvent::RELEASE;
            break;
        default:
            break;
    }
    #pragma GCC diagnostic pop

    return e;
}

/**
 * The tabs eat some mousePress events at random.
 *
 * Mitigate the issue by allowing the event series to begin later.
 */
bool Flickable::childMouseEventFilter(QQuickItem* item, QEvent* event)
{
    if (!d_ptr->m_Interactive)
        return false;

    const auto e = d_ptr->eventMapper(event);

    return e == FlickablePrivate::DragEvent::OTHER ?
        QQuickItem::childMouseEventFilter(item, event) :
        d_ptr->applyEvent(e, static_cast<QMouseEvent*>(event) );
}

bool Flickable::event(QEvent *event)
{
    if (!d_ptr->m_Interactive)
        return false;

    const auto e = d_ptr->eventMapper(event);

    if (event->type() == QEvent::Wheel) {
        setContentY(contentY() - static_cast<QWheelEvent*>(event)->angleDelta().y());
        event->accept();
        return true;
    }

    return e == FlickablePrivate::DragEvent::OTHER ?
        QQuickItem::event(event) :
        d_ptr->applyEvent(e, static_cast<QMouseEvent*>(event) );
}

void Flickable::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    if (d_ptr->m_pContainer) {
        d_ptr->m_pContainer->setWidth(std::max(newGeometry.width(), d_ptr->m_pContainer->width()));
        d_ptr->m_pContainer->setHeight(std::max(newGeometry.height(), d_ptr->m_pContainer->height()));

        emit contentHeightChanged(d_ptr->m_pContainer->height());
    }

    //TODO prevent out of scope
    QQuickItem::geometryChanged(newGeometry, oldGeometry);

    emit viewportChanged(viewport());
}

/// State functions ///

/**
 * Make the Mealy machine move between the states
 */
bool FlickablePrivate::applyEvent(DragEvent event, QMouseEvent* e)
{
    if (!m_pContainer)
        return false;

    const bool wasDragging(q_ptr->isDragging()), wasMoving(q_ptr->isMoving());

    // Set the state before the callback so recursive events work
    const int s = (int)m_State;
    m_State     = m_fStateMap            [s][(int)event];
    bool ret    = (this->*m_fStateMachine[s][(int)event])(e);

    if (ret && e)
        e->accept();

    if (wasDragging != q_ptr->isDragging())
        emit q_ptr->draggingChanged(q_ptr->isDragging());

    if (wasMoving != q_ptr->isMoving())
        emit q_ptr->movingChanged(q_ptr->isMoving());

    return ret && e;
}

bool FlickablePrivate::nothing(QMouseEvent*)
{
    return false;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn"
bool FlickablePrivate::error(QMouseEvent*)
{
    qWarning() << "simpleFlickable: Invalid state change";

    Q_ASSERT(false);
    return false;
}
#pragma GCC diagnostic pop

bool FlickablePrivate::stop(QMouseEvent* event)
{
    m_pTimer->stop();
    m_Velocity = 0;
    m_StartPoint = m_DragPoint  = {};

    // Resend for further processing
    if (event)
        applyEvent(FlickablePrivate::DragEvent::PRESS, event);

    return false;
}

bool FlickablePrivate::drag(QMouseEvent* e)
{
    if (!m_pContainer)
        return false;

    const int dy(e->pos().y() - m_DragPoint.y());
    m_DragPoint = e->pos();
    q_ptr->setContentY(q_ptr->contentY() - dy);

    // Reset the inertia on the differential inflexion points
    if ((m_LastDelta >= 0) ^ (dy >= 0)) {
        m_StartPoint = e->pos();
        m_StartTime  = QDateTime::currentMSecsSinceEpoch();
    }

    m_LastDelta = dy;

    return true;
}

bool FlickablePrivate::start(QMouseEvent* e)
{
    m_StartPoint = m_DragPoint = e->pos();
    m_StartTime  = QDateTime::currentMSecsSinceEpoch();

    q_ptr->setFocus(true, Qt::MouseFocusReason);

    // The event itself may be a normal click, let the children handle it too
    return false;
}

bool FlickablePrivate::cancel(QMouseEvent*)
{
    m_StartPoint = m_DragPoint = {};
    q_ptr->setKeepMouseGrab(false);

    // Reject the event, let the click pass though
    return false;
}

bool FlickablePrivate::release(QMouseEvent*)
{
    q_ptr->setKeepMouseGrab(false);
    q_ptr->ungrabMouse();

    if (updateVelocity())
        m_pTimer->start();
    else
        applyEvent(DragEvent::TIMEOUT, nullptr);

    m_DragPoint = {};

    return false;
}

bool FlickablePrivate::lock(QMouseEvent*)
{
    q_ptr->setKeepMouseGrab(true);
    q_ptr->grabMouse();

    return true;
}

bool FlickablePrivate::eval(QMouseEvent* e)
{
    // It might look like an oversimplification, but the math here is correct.
    // Think of the rectangle being at the origin of a radiant wheel. The
    // hypotenuse of the rectangle will point at an angle. This code is
    // equivalent to the range PI/2 <-> 3*(PI/2) U 5*(PI/2) <-> 7*(PI/2)

    static const constexpr uchar EVENT_THRESHOLD = 10;

    // Reject large horizontal swipe and allow large vertical ones
    if (std::fabs(m_StartPoint.x() - e->pos().x()) > EVENT_THRESHOLD) {
        applyEvent(DragEvent::REJECT, e);
        return false;
    }
    else if (std::fabs(m_StartPoint.y() - e->pos().y()) > EVENT_THRESHOLD)
        applyEvent(DragEvent::ACCEPT, e);

    return drag(e);
}

bool FlickablePrivate::inertia(QMouseEvent*)
{
    m_Velocity *= m_DecelRate;

    q_ptr->setContentY(q_ptr->contentY() - m_Velocity);

    // Clamp the asymptotes to avoid an infinite loop, I chose a random value
    if (std::fabs(m_Velocity) < 0.05)
        applyEvent(DragEvent::TIMEOUT, nullptr);

    return true;
}

bool Flickable::isDragging() const
{
    return d_ptr->m_State == FlickablePrivate::DragState::DRAGGED
        || d_ptr->m_State == FlickablePrivate::DragState::EVAL;
}

bool Flickable::isMoving() const
{
    return isDragging()
        || d_ptr->m_State == FlickablePrivate::DragState::INERTIA;
}

qreal Flickable::flickDeceleration() const
{
    return d_ptr->m_DecelRate;
}

void Flickable::setFlickDeceleration(qreal v)
{
    d_ptr->m_DecelRate = v;
}

bool Flickable::isInteractive() const
{
    return d_ptr->m_Interactive;
}

void Flickable::setInteractive(bool v)
{
    d_ptr->m_Interactive = v;
}

qreal Flickable::maximumFlickVelocity() const
{
    return d_ptr->m_MaxVelocity;
}

void Flickable::setMaximumFlickVelocity(qreal v)
{
    d_ptr->m_MaxVelocity = v;
}

QQmlContext* Flickable::rootContext() const
{
    if (!d_ptr->m_pRootContext)
        d_ptr->m_pRootContext = QQmlEngine::contextForObject(this);

    return d_ptr->m_pRootContext;
}

#include <flickable.moc>
