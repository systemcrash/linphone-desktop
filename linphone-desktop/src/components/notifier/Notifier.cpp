/*
 * Notifier.cpp
 * Copyright (C) 2017  Belledonne Communications, Grenoble, France
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  Created on: February 2, 2017
 *      Author: Ronan Abhamon
 */

#include "../../app/App.hpp"
#include "../../utils.hpp"
#include "../core/CoreManager.hpp"

#include "Notifier.hpp"

#include <QQmlComponent>
#include <QQuickWindow>
#include <QtDebug>
#include <QTimer>

// Notifications QML properties/methods.
#define NOTIFICATION_SHOW_METHOD_NAME "show"

#define NOTIFICATION_PROPERTY_DATA "notificationData"
#define NOTIFICATION_PROPERTY_HEIGHT "notificationHeight"
#define NOTIFICATION_PROPERTY_OFFSET "notificationOffset"

#define NOTIFICATION_PROPERTY_TIMER "__timer"

#define QML_NOTIFICATION_PATH_RECEIVED_MESSAGE "qrc:/ui/modules/Linphone/Notifications/NotificationReceivedMessage.qml"
#define QML_NOTIFICATION_PATH_RECEIVED_FILE_MESSAGE "qrc:/ui/modules/Linphone/Notifications/NotificationReceivedFileMessage.qml"
#define QML_NOTIFICATION_PATH_RECEIVED_CALL "qrc:/ui/modules/Linphone/Notifications/NotificationReceivedCall.qml"

#define NOTIFICATION_TIMEOUT_RECEIVED_MESSAGE 10000
#define NOTIFICATION_TIMEOUT_RECEIVED_FILE_MESSAGE 10000
#define NOTIFICATION_TIMEOUT_RECEIVED_CALL 30000

// Arbitrary hardcoded values.
#define NOTIFICATION_SPACING 10
#define N_MAX_NOTIFICATIONS 15
#define MAX_TIMEOUT 60000

// =============================================================================

inline int getNotificationSize (const QObject &object, const char *property) {
  QVariant variant = object.property(property);
  bool so_far_so_good;

  int size = variant.toInt(&so_far_so_good);
  if (!so_far_so_good || size < 0) {
    qWarning() << "Unable to get notification size.";
    return -1;
  }

  return size;
}

template<class T>
bool setProperty (QObject &object, const char *property, const T &value) {
  QVariant qvariant(value);

  if (!object.setProperty(property, qvariant)) {
    qWarning() << QStringLiteral("Unable to set property: `%1`.").arg(property);
    return false;
  }

  return true;
}

// -----------------------------------------------------------------------------

Notifier::Notifier (QObject *parent) :
  QObject(parent) {
  QQmlEngine *engine = App::getInstance()->getEngine();

  // Build components.
  m_components[Notifier::MessageReceived] = new QQmlComponent(engine, QUrl(QML_NOTIFICATION_PATH_RECEIVED_MESSAGE));
  m_components[Notifier::FileMessageReceived] = new QQmlComponent(engine, QUrl(QML_NOTIFICATION_PATH_RECEIVED_FILE_MESSAGE));
  m_components[Notifier::CallReceived] = new QQmlComponent(engine, QUrl(QML_NOTIFICATION_PATH_RECEIVED_CALL));

  // Check errors.
  for (int i = 0; i < Notifier::MaxNbTypes; ++i) {
    QQmlComponent *component = m_components[i];
    if (component->isError()) {
      qWarning() << QStringLiteral("Errors found in `Notification` component %1:").arg(i) << component->errors();
      abort();
    }
  }
}

Notifier::~Notifier () {
  for (int i = 0; i < Notifier::MaxNbTypes; ++i)
    delete m_components[i];
}

// -----------------------------------------------------------------------------

QObject *Notifier::createNotification (Notifier::NotificationType type) {
  m_mutex.lock();

  Q_ASSERT(m_n_instances <= N_MAX_NOTIFICATIONS);

  // Check existing instances.
  if (m_n_instances == N_MAX_NOTIFICATIONS) {
    qWarning() << "Unable to create another notification";
    m_mutex.unlock();
    return nullptr;
  }

  // Create instance and set attributes.
  QObject *object = m_components[type]->create();
  int offset = getNotificationSize(*object, NOTIFICATION_PROPERTY_HEIGHT);

  if (offset == -1 || !::setProperty(*object, NOTIFICATION_PROPERTY_OFFSET, m_offset)) {
    delete object;
    m_mutex.unlock();
    return nullptr;
  }

  m_offset = (offset + m_offset) + NOTIFICATION_SPACING;
  m_n_instances++;

  m_mutex.unlock();

  return object;
}

void Notifier::showNotification (QObject *notification, int timeout) {
  // Display notification.
  QMetaObject::invokeMethod(notification, NOTIFICATION_SHOW_METHOD_NAME, Qt::DirectConnection);

  QTimer *timer = new QTimer(notification);
  timer->setInterval(timeout > MAX_TIMEOUT ? MAX_TIMEOUT : timeout);
  timer->setSingleShot(true);
  notification->setProperty(NOTIFICATION_PROPERTY_TIMER, QVariant::fromValue(timer));

  // Destroy it after timeout.
  QObject::connect(
    timer, &QTimer::timeout, this, [this, notification]() {
      qDebug() << "toto";
      deleteNotification(QVariant::fromValue(notification));
    }
  );

  // Called explicitly (by a click on notification for example)
  QObject::connect(notification, SIGNAL(deleteNotification(QVariant)), this, SLOT(deleteNotification(QVariant)));

  timer->start();
}

// -----------------------------------------------------------------------------

void Notifier::deleteNotification (QVariant notification) {
  m_mutex.lock();

  QObject *instance = notification.value<QObject *>();

  // Notification marked destroyed.
  if (instance->property("__valid").isValid()) {
    m_mutex.unlock();
    return;
  }

  qDebug() << "Delete notification.";

  instance->setProperty("__valid", true);
  instance->property(NOTIFICATION_PROPERTY_TIMER).value<QTimer *>()->stop();

  m_n_instances--;
  if (m_n_instances == 0)
    m_offset = 0;

  Q_ASSERT(m_n_instances >= 0);

  m_mutex.unlock();

  instance->deleteLater();
}

// -----------------------------------------------------------------------------

void Notifier::notifyReceivedMessage (const shared_ptr<linphone::ChatMessage> &message) {
  QObject *notification = createNotification(Notifier::MessageReceived);
  if (!notification)
    return;

  QVariantMap map;
  map["message"] = ::Utils::linphoneStringToQString(message->getText());
  map["sipAddress"] = ::Utils::linphoneStringToQString(message->getFromAddress()->asStringUriOnly());
  map["window"].setValue(App::getInstance()->getMainWindow());

  ::setProperty(*notification, NOTIFICATION_PROPERTY_DATA, map);
  showNotification(notification, NOTIFICATION_TIMEOUT_RECEIVED_MESSAGE);
}

void Notifier::notifyReceivedFileMessage (const shared_ptr<linphone::ChatMessage> &message) {
  QObject *notification = createNotification(Notifier::FileMessageReceived);
  if (!notification)
    return;

  QVariantMap map;
  map["fileUri"] = ::Utils::linphoneStringToQString(message->getFileTransferFilepath());
  map["fileSize"] = static_cast<quint64>(message->getFileTransferInformation()->getSize());

  ::setProperty(*notification, NOTIFICATION_PROPERTY_DATA, map);
  showNotification(notification, NOTIFICATION_TIMEOUT_RECEIVED_FILE_MESSAGE);
}

void Notifier::notifyReceivedCall (const shared_ptr<linphone::Call> &call) {
  QObject *notification = createNotification(Notifier::CallReceived);
  if (!notification)
    return;

  CallModel *model = CoreManager::getInstance()->getCallsListModel()->getCall(call);

  QObject::connect(
    model, &CallModel::statusChanged, notification, [this, notification](CallModel::CallStatus status) {
      if (status == CallModel::CallStatusEnded || status == CallModel::CallStatusConnected)
        deleteNotification(QVariant::fromValue(notification));
    }
  );

  QVariantMap map;
  map["call"].setValue(model);

  ::setProperty(*notification, NOTIFICATION_PROPERTY_DATA, map);
  showNotification(notification, NOTIFICATION_TIMEOUT_RECEIVED_CALL);
}