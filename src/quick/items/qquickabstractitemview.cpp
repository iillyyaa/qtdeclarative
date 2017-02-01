/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickabstractitemview_p.h"
#include "qquickabstractitemview_p_p.h"

#include <QtQml/private/qqmlglobal_p.h>
#include <QtQml/qqmlcontext.h>
#include <QtQml/private/qqmldelegatemodel_p.h>
#include <QtQml/qqmlinfo.h>

QT_BEGIN_NAMESPACE

// Default cacheBuffer for all views.
#ifndef QML_VIEW_DEFAULTCACHEBUFFER
#define QML_VIEW_DEFAULTCACHEBUFFER 320
#endif

FxAbstractViewItem::FxAbstractViewItem(QQuickItem *i, QQuickAbstractItemView *v, bool own, QQuickAbstractItemViewAttached *attached)
    : item(i)
    , view(v)
    , transitionableItem(0)
    , attached(attached)
    , ownItem(own)
    , releaseAfterTransition(false)
    , trackGeom(false)
{
    if (attached) // can be null for default components (see createComponentItem)
        attached->setView(view);
}

FxAbstractViewItem::~FxAbstractViewItem()
{
    delete transitionableItem;
    if (ownItem && item) {
        trackGeometry(false);
        item->setParentItem(0);
        item->deleteLater();
        item = 0;
    }
}

qreal FxAbstractViewItem::itemX() const
{
    return transitionableItem ? transitionableItem->itemX() : (item ? item->x() : 0);
}

qreal FxAbstractViewItem::itemY() const
{
    return transitionableItem ? transitionableItem->itemY() : (item ? item->y() : 0);
}

void FxAbstractViewItem::moveTo(const QPointF &pos, bool immediate)
{
    if (transitionableItem)
        transitionableItem->moveTo(pos, immediate);
    else if (item)
        item->setPosition(pos);
}

void FxAbstractViewItem::setVisible(bool visible)
{
    if (!visible && transitionableItem && transitionableItem->transitionScheduledOrRunning())
        return;
    if (item)
        QQuickItemPrivate::get(item)->setCulled(!visible);
}

void FxAbstractViewItem::trackGeometry(bool track)
{
    if (track) {
        if (!trackGeom) {
            if (item) {
                QQuickItemPrivate *itemPrivate = QQuickItemPrivate::get(item);
                itemPrivate->addItemChangeListener(QQuickAbstractItemViewPrivate::get(view), QQuickItemPrivate::Geometry);
            }
            trackGeom = true;
        }
    } else {
        if (trackGeom) {
            if (item) {
                QQuickItemPrivate *itemPrivate = QQuickItemPrivate::get(item);
                itemPrivate->removeItemChangeListener(QQuickAbstractItemViewPrivate::get(view), QQuickItemPrivate::Geometry);
            }
            trackGeom = false;
        }
    }
}

QQuickItemViewTransitioner::TransitionType FxAbstractViewItem::scheduledTransitionType() const
{
    return transitionableItem ? transitionableItem->nextTransitionType : QQuickItemViewTransitioner::NoTransition;
}

bool FxAbstractViewItem::transitionScheduledOrRunning() const
{
    return transitionableItem ? transitionableItem->transitionScheduledOrRunning() : false;
}

bool FxAbstractViewItem::transitionRunning() const
{
    return transitionableItem ? transitionableItem->transitionRunning() : false;
}

bool FxAbstractViewItem::isPendingRemoval() const
{
    return transitionableItem ? transitionableItem->isPendingRemoval() : false;
}

void FxAbstractViewItem::transitionNextReposition(QQuickItemViewTransitioner *transitioner, QQuickItemViewTransitioner::TransitionType type, bool asTarget)
{
    if (!transitioner)
        return;
    if (!transitionableItem)
        transitionableItem = new QQuickItemViewTransitionableItem(item);
    transitioner->transitionNextReposition(transitionableItem, type, asTarget);
}

bool FxAbstractViewItem::prepareTransition(QQuickItemViewTransitioner *transitioner, const QRectF &viewBounds)
{
    return transitionableItem ? transitionableItem->prepareTransition(transitioner, index, viewBounds) : false;
}

void FxAbstractViewItem::startTransition(QQuickItemViewTransitioner *transitioner)
{
    if (transitionableItem)
        transitionableItem->startTransition(transitioner, index);
}

QQuickAbstractItemViewPrivate::QQuickAbstractItemViewPrivate()
    : itemCount(0),
      buffer(QML_VIEW_DEFAULTCACHEBUFFER),
      bufferMode(BufferBefore | BufferAfter),
      layoutDirection(Qt::LeftToRight),
      verticalLayoutDirection(QQuickItemView::TopToBottom), // ###
      moveReason(Other),
      visibleIndex(0),
      currentIndex(-1),
      currentItem(nullptr),
      trackedItem(nullptr),
      requestedIndex(-1),
      highlightComponent(nullptr),
      highlight(nullptr),
      highlightRange(QQuickItemView::NoHighlightRange), // ###
      highlightMoveDuration(150),
      transitioner(nullptr),
      minExtent(0),
      maxExtent(0),
      ownModel(false),
      wrap(false),
      keyNavigationEnabled(true),
      explicitKeyNavigationEnabled(false),
      inLayout(false),
      inViewportMoved(false),
      forceLayout(false),
      currentIndexCleared(false),
      haveHighlightRange(false),
      autoHighlight(true),
      fillCacheBuffer(false),
      inRequest(false),
      runDelayedRemoveTransition(false),
      delegateValidated(false)
{
    bufferPause.addAnimationChangeListener(this, QAbstractAnimationJob::Completion);
    bufferPause.setLoopCount(1);
    bufferPause.setDuration(16);
}

QQuickAbstractItemViewPrivate::~QQuickAbstractItemViewPrivate()
{
    if (transitioner)
        transitioner->setChangeListener(nullptr);
    delete transitioner;
}

bool QQuickAbstractItemViewPrivate::isValid() const
{
    return model && model->count() && model->isValid();
}

void QQuickAbstractItemViewPrivate::init()
{
    Q_Q(QQuickAbstractItemView);
    q->setFlag(QQuickItem::ItemIsFocusScope);
    QObject::connect(q, SIGNAL(movementEnded()), q, SLOT(animStopped()));
    QObject::connect(q, &QQuickFlickable::interactiveChanged, q, &QQuickAbstractItemView::keyNavigationEnabledChanged);
}

void QQuickAbstractItemViewPrivate::clear()
{
    currentChanges.reset();
    timeline.clear();

    for (FxAbstractViewItem *item : qAsConst(releasePendingTransition)) {
        item->releaseAfterTransition = false;
        releaseItem(item);
    }
    releasePendingTransition.clear();

    releaseItem(currentItem);
    currentItem = 0;
    createHighlight();
    trackedItem = 0;

    if (requestedIndex >= 0) {
        if (model)
            model->cancel(requestedIndex);
        requestedIndex = -1;
    }

    markExtentsDirty();
    itemCount = 0;
}

void QQuickAbstractItemViewPrivate::updateViewport()
{
}

void QQuickAbstractItemViewPrivate::resetPosition()
{
}

void QQuickAbstractItemViewPrivate::regenerate()
{
}

void QQuickAbstractItemViewPrivate::layout()
{
}

void QQuickAbstractItemViewPrivate::refill()
{
}

void QQuickAbstractItemViewPrivate::animationFinished(QAbstractAnimationJob *)
{
    Q_Q(QQuickAbstractItemView);
    fillCacheBuffer = true;
    q->polish();
}

void QQuickAbstractItemViewPrivate::mirrorChange()
{
    Q_Q(QQuickAbstractItemView);
    regenerate();
    emit q->effectiveLayoutDirectionChanged();
}

/*
  This may return 0 if the item is being created asynchronously.
  When the item becomes available, refill() will be called and the item
  will be returned on the next call to createItem().
*/
FxAbstractViewItem *QQuickAbstractItemViewPrivate::createItem(int modelIndex, bool asynchronous)
{
    Q_Q(QQuickAbstractItemView);

    if (requestedIndex == modelIndex && asynchronous)
        return 0;

    for (int i=0; i<releasePendingTransition.count(); i++) {
        if (releasePendingTransition.at(i)->index == modelIndex
                && !releasePendingTransition.at(i)->isPendingRemoval()) {
            releasePendingTransition[i]->releaseAfterTransition = false;
            return releasePendingTransition.takeAt(i);
        }
    }

    if (asynchronous)
        requestedIndex = modelIndex;
    inRequest = true;

    QObject* object = model->object(modelIndex, asynchronous);
    QQuickItem *item = qmlobject_cast<QQuickItem*>(object);
    if (!item) {
        if (object) {
            model->release(object);
            if (!delegateValidated) {
                delegateValidated = true;
                QObject* delegate = q->delegate();
                qmlWarning(delegate ? delegate : q) << QQuickAbstractItemView::tr("Delegate must be of Item type"); // ###
            }
        }
        inRequest = false;
        return 0;
    } else {
        item->setParentItem(q->contentItem());
        if (requestedIndex == modelIndex)
            requestedIndex = -1;
        FxAbstractViewItem *viewItem = newViewItem(modelIndex, item);
        if (viewItem) {
            viewItem->index = modelIndex;
            // do other set up for the new item that should not happen
            // until after bindings are evaluated
            initializeViewItem(viewItem);
            unrequestedItems.remove(item);
        }
        inRequest = false;
        return viewItem;
    }
}

bool QQuickAbstractItemViewPrivate::releaseItem(FxAbstractViewItem *item)
{
    Q_Q(QQuickAbstractItemView);
    if (!item || !model)
        return true;
    if (trackedItem == item)
        trackedItem = 0;
    item->trackGeometry(false);

    QQmlInstanceModel::ReleaseFlags flags = model->release(item->item);
    if (item->item) {
        if (flags == 0) {
            // item was not destroyed, and we no longer reference it.
            QQuickItemPrivate::get(item->item)->setCulled(true);
            unrequestedItems.insert(item->item, model->indexOf(item->item, q));
        } else if (flags & QQmlInstanceModel::Destroyed) {
            item->item->setParentItem(0);
        }
    }
    delete item;
    return flags != QQmlInstanceModel::Referenced;
}

QQuickItem *QQuickAbstractItemViewPrivate::createHighlightItem() const
{
    return createComponentItem(highlightComponent, 0.0, true);
}

QQuickItem *QQuickAbstractItemViewPrivate::createComponentItem(QQmlComponent *component, qreal zValue, bool createDefault) const
{
    Q_Q(const QQuickAbstractItemView);

    QQuickItem *item = 0;
    if (component) {
        QQmlContext *creationContext = component->creationContext();
        QQmlContext *context = new QQmlContext(
                creationContext ? creationContext : qmlContext(q));
        QObject *nobj = component->beginCreate(context);
        if (nobj) {
            QQml_setParent_noEvent(context, nobj);
            item = qobject_cast<QQuickItem *>(nobj);
            if (!item)
                delete nobj;
        } else {
            delete context;
        }
    } else if (createDefault) {
        item = new QQuickItem;
    }
    if (item) {
        if (qFuzzyIsNull(item->z()))
            item->setZ(zValue);
        QQml_setParent_noEvent(item, q->contentItem());
        item->setParentItem(q->contentItem());
    }
    if (component)
        component->completeCreate();
    return item;
}

void QQuickAbstractItemViewPrivate::updateCurrent(int modelIndex)
{
    Q_Q(QQuickAbstractItemView);
    applyPendingChanges();
    if (!q->isComponentComplete() || !isValid() || modelIndex < 0 || modelIndex >= model->count()) {
        if (currentItem) {
            if (currentItem->attached)
                currentItem->attached->setIsCurrentItem(false);
            releaseItem(currentItem);
            currentItem = 0;
            currentIndex = modelIndex;
            emit q->currentIndexChanged();
            emit q->currentItemChanged();
            updateHighlight();
        } else if (currentIndex != modelIndex) {
            currentIndex = modelIndex;
            emit q->currentIndexChanged();
        }
        return;
    }

    if (currentItem && currentIndex == modelIndex) {
        updateHighlight();
        return;
    }

    FxAbstractViewItem *oldCurrentItem = currentItem;
    int oldCurrentIndex = currentIndex;
    currentIndex = modelIndex;
    currentItem = createItem(modelIndex, false);
    if (oldCurrentItem && oldCurrentItem->attached && (!currentItem || oldCurrentItem->item != currentItem->item))
        oldCurrentItem->attached->setIsCurrentItem(false);
    if (currentItem) {
        currentItem->item->setFocus(true);
        if (currentItem->attached)
            currentItem->attached->setIsCurrentItem(true);
        initializeCurrentItem();
    }

    updateHighlight();
    if (oldCurrentIndex != currentIndex)
        emit q->currentIndexChanged();
    if (oldCurrentItem != currentItem
            && (!oldCurrentItem || !currentItem || oldCurrentItem->item != currentItem->item))
        emit q->currentItemChanged();
    releaseItem(oldCurrentItem);
}

void QQuickAbstractItemViewPrivate::updateTrackedItem()
{
    Q_Q(QQuickAbstractItemView);
    FxAbstractViewItem *item = currentItem;
    if (highlight)
        item = highlight;
    trackedItem = item;

    if (trackedItem)
        q->trackedPositionChanged();
}

void QQuickAbstractItemViewPrivate::updateUnrequestedIndexes()
{
    Q_Q(QQuickAbstractItemView);
    for (QHash<QQuickItem *, int>::iterator it = unrequestedItems.begin(), end = unrequestedItems.end(); it != end; ++it)
        *it = model->indexOf(it.key(), q);
}

void QQuickAbstractItemViewPrivate::updateUnrequestedPositions()
{
    for (QHash<QQuickItem *, int>::const_iterator it = unrequestedItems.cbegin(), cend = unrequestedItems.cend(); it != cend; ++it) {
        if (it.value() >= 0)
            repositionPackageItemAt(it.key(), it.value());
    }
}

bool QQuickAbstractItemViewPrivate::createOwnModel()
{
    Q_Q(QQuickAbstractItemView);
    if (!ownModel) {
        model = new QQmlDelegateModel(qmlContext(q));
        ownModel = true;
        if (q->isComponentComplete())
            static_cast<QQmlDelegateModel *>(model.data())->componentComplete();
        return true;
    }
    return false;
}

void QQuickAbstractItemViewPrivate::destroyOwnModel()
{
    if (ownModel) {
        delete model;
        ownModel = false;
    }
}

void QQuickAbstractItemViewPrivate::applyPendingChanges()
{
    Q_Q(QQuickAbstractItemView);
    if (q->isComponentComplete() && currentChanges.hasPendingChanges())
        layout();
}

void QQuickAbstractItemViewPrivate::createTransitioner()
{
    if (!transitioner) {
        transitioner = new QQuickItemViewTransitioner;
        transitioner->setChangeListener(this);
    }
}

void QQuickAbstractItemViewPrivate::prepareRemoveTransitions(QHash<QQmlChangeSet::MoveKey, FxAbstractViewItem *> *removedItems)
{
    if (!transitioner)
        return;

    if (transitioner->canTransition(QQuickItemViewTransitioner::RemoveTransition, true)
            || transitioner->canTransition(QQuickItemViewTransitioner::RemoveTransition, false)) {
        for (QHash<QQmlChangeSet::MoveKey, FxAbstractViewItem *>::Iterator it = removedItems->begin();
             it != removedItems->end(); ) {
            bool isRemove = it.key().moveId < 0;
            if (isRemove) {
                FxAbstractViewItem *item = *it;
                item->trackGeometry(false);
                item->releaseAfterTransition = true;
                releasePendingTransition.append(item);
                item->transitionNextReposition(transitioner, QQuickItemViewTransitioner::RemoveTransition, true);
                it = removedItems->erase(it);
            } else {
                ++it;
            }
        }
    }
}

bool QQuickAbstractItemViewPrivate::prepareNonVisibleItemTransition(FxAbstractViewItem *item, const QRectF &viewBounds)
{
    // Called for items that have been removed from visibleItems and may now be
    // transitioned out of the view. This applies to items that are being directly
    // removed, or moved to outside of the view, as well as those that are
    // displaced to a position outside of the view due to an insert or move.

    if (!transitioner)
        return false;

    if (item->scheduledTransitionType() == QQuickItemViewTransitioner::MoveTransition)
        repositionItemAt(item, item->index, 0);

    if (item->prepareTransition(transitioner, viewBounds)) {
        item->releaseAfterTransition = true;
        return true;
    }
    return false;
}

void QQuickAbstractItemViewPrivate::viewItemTransitionFinished(QQuickItemViewTransitionableItem *item)
{
    for (int i = 0; i < releasePendingTransition.count(); ++i) {
        if (releasePendingTransition.at(i)->transitionableItem == item) {
            releaseItem(releasePendingTransition.takeAt(i));
            return;
        }
    }
}

QQuickAbstractItemView::QQuickAbstractItemView(QQuickFlickablePrivate &dd, QQuickItem *parent)
    : QQuickFlickable(dd, parent)
{
}

QQuickAbstractItemView::~QQuickAbstractItemView()
{
}

QQuickItem *QQuickAbstractItemView::currentItem() const
{
    Q_D(const QQuickAbstractItemView);
    return d->currentItem ? d->currentItem->item : 0;
}

QVariant QQuickAbstractItemView::model() const
{
    Q_D(const QQuickAbstractItemView);
    return d->modelVariant;
}

void QQuickAbstractItemView::setModel(const QVariant &m)
{
    Q_D(QQuickAbstractItemView);
    QVariant model = m;
    if (model.userType() == qMetaTypeId<QJSValue>())
        model = model.value<QJSValue>().toVariant();

    if (d->modelVariant == model)
        return;
    if (d->model) {
        disconnect(d->model, SIGNAL(modelUpdated(QQmlChangeSet,bool)),
                this, SLOT(modelUpdated(QQmlChangeSet,bool)));
        disconnect(d->model, SIGNAL(initItem(int,QObject*)), this, SLOT(initItem(int,QObject*)));
        disconnect(d->model, SIGNAL(createdItem(int,QObject*)), this, SLOT(createdItem(int,QObject*)));
        disconnect(d->model, SIGNAL(destroyingItem(QObject*)), this, SLOT(destroyingItem(QObject*)));
    }

    QQmlInstanceModel *oldModel = d->model;

    d->clear();
    d->model = 0;
    d->resetPosition();
    d->modelVariant = model;

    QObject *object = qvariant_cast<QObject*>(model);
    QQmlInstanceModel *vim = 0;
    if (object && (vim = qobject_cast<QQmlInstanceModel *>(object))) {
        d->destroyOwnModel();
        d->model = vim;
    } else {
        if (!d->createOwnModel())
            d->model = oldModel;
        if (QQmlDelegateModel *dataModel = qobject_cast<QQmlDelegateModel*>(d->model))
            dataModel->setModel(model);
    }

    if (d->model) {
        d->bufferMode = QQuickAbstractItemViewPrivate::BufferBefore | QQuickAbstractItemViewPrivate::BufferAfter;
        connect(d->model, SIGNAL(createdItem(int,QObject*)), this, SLOT(createdItem(int,QObject*)));
        connect(d->model, SIGNAL(initItem(int,QObject*)), this, SLOT(initItem(int,QObject*)));
        connect(d->model, SIGNAL(destroyingItem(QObject*)), this, SLOT(destroyingItem(QObject*)));
        if (isComponentComplete()) {
            d->updateSectionCriteria();
            d->refill();
            d->currentIndex = -1;
            setCurrentIndex(d->model->count() > 0 ? 0 : -1);
            d->updateViewport();

            if (d->transitioner && d->transitioner->populateTransition) {
                d->transitioner->setPopulateTransitionEnabled(true);
                d->forceLayoutPolish();
            }
        }

        connect(d->model, SIGNAL(modelUpdated(QQmlChangeSet,bool)),
                this, SLOT(modelUpdated(QQmlChangeSet,bool)));
        emit countChanged();
    }
    emit modelChanged();
}

QQmlComponent *QQuickAbstractItemView::delegate() const
{
    Q_D(const QQuickAbstractItemView);
    if (d->model) {
        if (QQmlDelegateModel *dataModel = qobject_cast<QQmlDelegateModel*>(d->model))
            return dataModel->delegate();
    }

    return 0;
}

int QQuickAbstractItemView::count() const
{
    Q_D(const QQuickAbstractItemView);
    if (!d->model)
        return 0;
    return d->model->count();
}

int QQuickAbstractItemView::currentIndex() const
{
    Q_D(const QQuickAbstractItemView);
    return d->currentIndex;
}

void QQuickAbstractItemView::setCurrentIndex(int index)
{
    Q_D(QQuickAbstractItemView);
    if (d->inRequest)  // currently creating item
        return;
    d->currentIndexCleared = (index == -1);

    d->applyPendingChanges();
    if (index == d->currentIndex)
        return;
    if (isComponentComplete() && d->isValid()) {
        d->moveReason = QQuickAbstractItemViewPrivate::SetIndex;
        d->updateCurrent(index);
    } else if (d->currentIndex != index) {
        d->currentIndex = index;
        emit currentIndexChanged();
    }
}

int QQuickAbstractItemView::cacheBuffer() const
{
    Q_D(const QQuickAbstractItemView);
    return d->buffer;
}

void QQuickAbstractItemView::setCacheBuffer(int b)
{
    Q_D(QQuickAbstractItemView);
    if (b < 0) {
        qmlWarning(this) << "Cannot set a negative cache buffer";
        return;
    }

    if (d->buffer != b) {
        d->buffer = b;
        if (isComponentComplete()) {
            d->bufferMode = QQuickAbstractItemViewPrivate::BufferBefore | QQuickAbstractItemViewPrivate::BufferAfter;
            d->refillOrLayout();
        }
        emit cacheBufferChanged();
    }
}

bool QQuickAbstractItemView::isWrapEnabled() const
{
    Q_D(const QQuickAbstractItemView);
    return d->wrap;
}

void QQuickAbstractItemView::setWrapEnabled(bool wrap)
{
    Q_D(QQuickAbstractItemView);
    if (d->wrap == wrap)
        return;
    d->wrap = wrap;
    emit keyNavigationWrapsChanged();
}

bool QQuickAbstractItemView::isKeyNavigationEnabled() const
{
    Q_D(const QQuickAbstractItemView);
    return d->explicitKeyNavigationEnabled ? d->keyNavigationEnabled : d->interactive;
}

void QQuickAbstractItemView::setKeyNavigationEnabled(bool keyNavigationEnabled)
{
    // TODO: default binding to "interactive" can be removed in Qt 6; it only exists for compatibility reasons.
    Q_D(QQuickAbstractItemView);
    const bool wasImplicit = !d->explicitKeyNavigationEnabled;
    if (wasImplicit)
        QObject::disconnect(this, &QQuickFlickable::interactiveChanged, this, &QQuickAbstractItemView::keyNavigationEnabledChanged);

    d->explicitKeyNavigationEnabled = true;

    // Ensure that we emit the change signal in case there is no different in value.
    if (d->keyNavigationEnabled != keyNavigationEnabled || wasImplicit) {
        d->keyNavigationEnabled = keyNavigationEnabled;
        emit keyNavigationEnabledChanged();
    }
}

Qt::LayoutDirection QQuickAbstractItemView::layoutDirection() const
{
    Q_D(const QQuickAbstractItemView);
    return d->layoutDirection;
}

void QQuickAbstractItemView::setLayoutDirection(Qt::LayoutDirection layoutDirection)
{
    Q_D(QQuickAbstractItemView);
    if (d->layoutDirection != layoutDirection) {
        d->layoutDirection = layoutDirection;
        d->regenerate();
        emit layoutDirectionChanged();
        emit effectiveLayoutDirectionChanged();
    }
}

Qt::LayoutDirection QQuickAbstractItemView::effectiveLayoutDirection() const
{
    Q_D(const QQuickAbstractItemView);
    if (d->effectiveLayoutMirror)
        return d->layoutDirection == Qt::RightToLeft ? Qt::LeftToRight : Qt::RightToLeft;
    else
        return d->layoutDirection;
}

QQuickAbstractItemView::VerticalLayoutDirection QQuickAbstractItemView::verticalLayoutDirection() const
{
    Q_D(const QQuickAbstractItemView);
    return d->verticalLayoutDirection;
}

void QQuickAbstractItemView::setVerticalLayoutDirection(VerticalLayoutDirection layoutDirection)
{
    Q_D(QQuickAbstractItemView);
    if (d->verticalLayoutDirection != layoutDirection) {
        d->verticalLayoutDirection = layoutDirection;
        d->regenerate();
        emit verticalLayoutDirectionChanged();
    }
}

QQuickTransition *QQuickAbstractItemView::populateTransition() const
{
    Q_D(const QQuickAbstractItemView);
    return d->transitioner ? d->transitioner->populateTransition : 0;
}

void QQuickAbstractItemView::setPopulateTransition(QQuickTransition *transition)
{
    Q_D(QQuickAbstractItemView);
    d->createTransitioner();
    if (d->transitioner->populateTransition != transition) {
        d->transitioner->populateTransition = transition;
        emit populateTransitionChanged();
    }
}

QQuickTransition *QQuickAbstractItemView::addTransition() const
{
    Q_D(const QQuickAbstractItemView);
    return d->transitioner ? d->transitioner->addTransition : 0;
}

void QQuickAbstractItemView::setAddTransition(QQuickTransition *transition)
{
    Q_D(QQuickAbstractItemView);
    d->createTransitioner();
    if (d->transitioner->addTransition != transition) {
        d->transitioner->addTransition = transition;
        emit addTransitionChanged();
    }
}

QQuickTransition *QQuickAbstractItemView::addDisplacedTransition() const
{
    Q_D(const QQuickAbstractItemView);
    return d->transitioner ? d->transitioner->addDisplacedTransition : 0;
}

void QQuickAbstractItemView::setAddDisplacedTransition(QQuickTransition *transition)
{
    Q_D(QQuickAbstractItemView);
    d->createTransitioner();
    if (d->transitioner->addDisplacedTransition != transition) {
        d->transitioner->addDisplacedTransition = transition;
        emit addDisplacedTransitionChanged();
    }
}

QQuickTransition *QQuickAbstractItemView::moveTransition() const
{
    Q_D(const QQuickAbstractItemView);
    return d->transitioner ? d->transitioner->moveTransition : 0;
}

void QQuickAbstractItemView::setMoveTransition(QQuickTransition *transition)
{
    Q_D(QQuickAbstractItemView);
    d->createTransitioner();
    if (d->transitioner->moveTransition != transition) {
        d->transitioner->moveTransition = transition;
        emit moveTransitionChanged();
    }
}

QQuickTransition *QQuickAbstractItemView::moveDisplacedTransition() const
{
    Q_D(const QQuickAbstractItemView);
    return d->transitioner ? d->transitioner->moveDisplacedTransition : 0;
}

void QQuickAbstractItemView::setMoveDisplacedTransition(QQuickTransition *transition)
{
    Q_D(QQuickAbstractItemView);
    d->createTransitioner();
    if (d->transitioner->moveDisplacedTransition != transition) {
        d->transitioner->moveDisplacedTransition = transition;
        emit moveDisplacedTransitionChanged();
    }
}

QQuickTransition *QQuickAbstractItemView::removeTransition() const
{
    Q_D(const QQuickAbstractItemView);
    return d->transitioner ? d->transitioner->removeTransition : 0;
}

void QQuickAbstractItemView::setRemoveTransition(QQuickTransition *transition)
{
    Q_D(QQuickAbstractItemView);
    d->createTransitioner();
    if (d->transitioner->removeTransition != transition) {
        d->transitioner->removeTransition = transition;
        emit removeTransitionChanged();
    }
}

QQuickTransition *QQuickAbstractItemView::removeDisplacedTransition() const
{
    Q_D(const QQuickAbstractItemView);
    return d->transitioner ? d->transitioner->removeDisplacedTransition : 0;
}

void QQuickAbstractItemView::setRemoveDisplacedTransition(QQuickTransition *transition)
{
    Q_D(QQuickAbstractItemView);
    d->createTransitioner();
    if (d->transitioner->removeDisplacedTransition != transition) {
        d->transitioner->removeDisplacedTransition = transition;
        emit removeDisplacedTransitionChanged();
    }
}

QQuickTransition *QQuickAbstractItemView::displacedTransition() const
{
    Q_D(const QQuickAbstractItemView);
    return d->transitioner ? d->transitioner->displacedTransition : 0;
}

void QQuickAbstractItemView::setDisplacedTransition(QQuickTransition *transition)
{
    Q_D(QQuickAbstractItemView);
    d->createTransitioner();
    if (d->transitioner->displacedTransition != transition) {
        d->transitioner->displacedTransition = transition;
        emit displacedTransitionChanged();
    }
}

QQmlComponent *QQuickAbstractItemView::highlight() const
{
    Q_D(const QQuickAbstractItemView);
    return d->highlightComponent;
}

void QQuickAbstractItemView::setHighlight(QQmlComponent *highlightComponent)
{
    Q_D(QQuickAbstractItemView);
    if (highlightComponent != d->highlightComponent) {
        d->applyPendingChanges();
        d->highlightComponent = highlightComponent;
        d->createHighlight();
        if (d->currentItem)
            d->updateHighlight();
        emit highlightChanged();
    }
}

QQuickItem *QQuickAbstractItemView::highlightItem() const
{
    Q_D(const QQuickAbstractItemView);
    return d->highlight ? d->highlight->item : 0;
}

bool QQuickAbstractItemView::highlightFollowsCurrentItem() const
{
    Q_D(const QQuickAbstractItemView);
    return d->autoHighlight;
}

void QQuickAbstractItemView::setHighlightFollowsCurrentItem(bool autoHighlight)
{
    Q_D(QQuickAbstractItemView);
    if (d->autoHighlight != autoHighlight) {
        d->autoHighlight = autoHighlight;
        if (autoHighlight)
            d->updateHighlight();
        emit highlightFollowsCurrentItemChanged();
    }
}

int QQuickAbstractItemView::highlightMoveDuration() const
{
    Q_D(const QQuickAbstractItemView);
    return d->highlightMoveDuration;
}

void QQuickAbstractItemView::setHighlightMoveDuration(int duration)
{
    Q_D(QQuickAbstractItemView);
    if (d->highlightMoveDuration != duration) {
        d->highlightMoveDuration = duration;
        emit highlightMoveDurationChanged();
    }
}

void QQuickAbstractItemView::forceLayout()
{
    Q_D(QQuickAbstractItemView);
    if (isComponentComplete() && (d->currentChanges.hasPendingChanges() || d->forceLayout))
        d->layout();
}

void QQuickAbstractItemView::updatePolish()
{
    Q_D(QQuickAbstractItemView);
    QQuickFlickable::updatePolish();
    d->layout();
}

void QQuickAbstractItemView::initItem(int, QObject *object)
{
    QQuickItem* item = qmlobject_cast<QQuickItem*>(object);
    if (item) {
        if (qFuzzyIsNull(item->z()))
            item->setZ(1);
        item->setParentItem(contentItem());
        QQuickItemPrivate::get(item)->setCulled(true);
    }
}

void QQuickAbstractItemView::modelUpdated(const QQmlChangeSet &changeSet, bool reset)
{
    Q_D(QQuickAbstractItemView);
    if (reset) {
        if (d->transitioner)
            d->transitioner->setPopulateTransitionEnabled(true);
        d->moveReason = QQuickAbstractItemViewPrivate::SetIndex;
        d->regenerate();
        if (d->highlight && d->currentItem) {
            if (d->autoHighlight)
                d->resetHighlightPosition();
            d->updateTrackedItem();
        }
        d->moveReason = QQuickAbstractItemViewPrivate::Other;
        emit countChanged();
        if (d->transitioner && d->transitioner->populateTransition)
            d->forceLayoutPolish();
    } else {
        if (d->inLayout) {
            d->bufferedChanges.prepare(d->currentIndex, d->itemCount);
            d->bufferedChanges.applyChanges(changeSet);
        } else {
            if (d->bufferedChanges.hasPendingChanges()) {
                d->currentChanges.applyBufferedChanges(d->bufferedChanges);
                d->bufferedChanges.reset();
            }
            d->currentChanges.prepare(d->currentIndex, d->itemCount);
            d->currentChanges.applyChanges(changeSet);
        }
        polish();
    }
}

void QQuickAbstractItemView::destroyingItem(QObject *object)
{
    Q_D(QQuickAbstractItemView);
    QQuickItem* item = qmlobject_cast<QQuickItem*>(object);
    if (item) {
        item->setParentItem(0);
        d->unrequestedItems.remove(item);
    }
}

void QQuickAbstractItemView::animStopped()
{
    Q_D(QQuickAbstractItemView);
    d->bufferMode = QQuickAbstractItemViewPrivate::BufferBefore | QQuickAbstractItemViewPrivate::BufferAfter;
    d->refillOrLayout();
    if (d->haveHighlightRange && d->highlightRange == QQuickItemView::StrictlyEnforceRange) // ###
        d->updateHighlight();
}

void QQuickAbstractItemView::trackedPositionChanged()
{
}

QT_END_NAMESPACE
