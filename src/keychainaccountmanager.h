/*
 * SPDX-FileCopyrightText: 2014 Daniel Vrátil <dvratil@redhat.com>
 * SPDX-FileCopyrightText: 2016 Elvis Angelaccio <elvis.angelaccio@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#pragma once

#include "abstractaccountmanager.h"

namespace QKeychain
{
class Job;
}

class KeychainAccountManager : public AbstractAccountManager
{
public:
    virtual ~KeychainAccountManager()
    {
    }

    KGAPI2::AccountPtr account(const QString &accountName) override;
    KGAPI2::AccountPtr createAccount() override;
    KGAPI2::AccountPtr refreshAccount(const KGAPI2::AccountPtr &account) override;
    void removeAccount(const QString &accountName) override;
    QSet<QString> accounts() override;

private:
    template<typename T>
    QByteArray serialize(const T &object);

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
