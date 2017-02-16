/*
 * Copyright (C) 2014  Daniel Vrátil <dvratil@redhat.com>
 * Copyright (c) 2016 Elvis Angelaccio <elvis.angelaccio@kde.org>
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

#include "abstractaccountmanager.h"

namespace QKeychain
{
    class Job;
}

class AccountManager : public AbstractAccountManager
{
public:
    virtual ~AccountManager() {}

    /**
     * @return Pointer to the account for @p accountName.
     * If @p accountName is empty, an authentication job will attempt
     * to retrieve the credentials.
     */
    KGAPI2::AccountPtr account(const QString &accountName) override;

    KGAPI2::AccountPtr refreshAccount(const KGAPI2::AccountPtr &account) override;
    void removeAccount(const QString &accountName) override;
    QSet<QString> accounts() override;

private:
    template<typename T>
    QByteArray serialize(const T& object);

    template<typename T>
    T deserialize(QByteArray *data);

    void storeAccount(const KGAPI2::AccountPtr &account);

    // Store/remove account names in/from gdrive-accounts keychain entry.
    void removeAccountName(const QString &accountName);
    void storeAccountName(const QString &accountName);

    QMap<QString, QString> readMap(const QString &accountName);
    void writeMap(const QString &accountName, const QMap<QString, QString> &map);
    void runKeychainJob(QKeychain::Job *job);

    QSet<QString> m_accounts;

    static QString s_apiKey;
    static QString s_apiSecret;
};

#endif // ACCOUNTMANAGER_H
