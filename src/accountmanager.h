/*
 * Copyright (C) 2014  Daniel Vrátil <dvratil@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef ACCOUNTMANAGER_H
#define ACCOUNTMANAGER_H

#include <QStringList>

#include <KWallet/Wallet>
#include <kio/udsentry.h>

#include <LibKGAPI2/Account>

class AccountManager
{
public:
    AccountManager();
    ~AccountManager();

    KGAPI2::AccountPtr account(const QString &accountName);
    void storeAccount(const KGAPI2::AccountPtr &account);
    KGAPI2::AccountPtr refreshAccount(const KGAPI2::AccountPtr &account);

    QStringList accounts();

    static KIO::UDSEntry accountToUDSEntry(const QString &accountName);

private:

    KGAPI2::AccountPtr m_account;

    KWallet::Wallet *m_wallet;

    static QString s_apiKey;
    static QString s_apiSecret;
};

#endif // ACCOUNTMANAGER_H
