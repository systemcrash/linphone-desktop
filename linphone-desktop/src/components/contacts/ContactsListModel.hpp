/*
 * ContactsListModel.hpp
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

#ifndef CONTACTS_LIST_MODEL_H_
#define CONTACTS_LIST_MODEL_H_

#include <linphone++/linphone.hh>

#include "../contact/ContactModel.hpp"

#include <QAbstractListModel>

// =============================================================================

class ContactsListModel : public QAbstractListModel {
  friend class SipAddressesModel;

  Q_OBJECT;

public:
  ContactsListModel (QObject *parent = Q_NULLPTR);
  ~ContactsListModel () = default;

  int rowCount (const QModelIndex &index = QModelIndex()) const override;

  QHash<int, QByteArray> roleNames () const override;
  QVariant data (const QModelIndex &index, int role = Qt::DisplayRole) const override;

  bool removeRow (int row, const QModelIndex &parent = QModelIndex());
  bool removeRows (int row, int count, const QModelIndex &parent = QModelIndex()) override;

  Q_INVOKABLE ContactModel *addContact (VcardModel *vcard);
  Q_INVOKABLE void removeContact (ContactModel *contact);

signals:
  void contactAdded (ContactModel *contact);
  void contactRemoved (const ContactModel *contact);
  void contactUpdated (ContactModel *contact);

  void sipAddressAdded (ContactModel *contact, const QString &sip_address);
  void sipAddressRemoved (ContactModel *contact, const QString &sip_address);

private:
  void addContact (ContactModel *contact);

  QList<ContactModel *> m_list;
  std::shared_ptr<linphone::FriendList> m_linphone_friends;
};

#endif // CONTACTS_LIST_MODEL_H_