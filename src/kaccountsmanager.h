/*
 * SPDX-FileCopyrightText: 2017 Elvis Angelaccio <elvis.angelaccio@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#pragma once

#include "abstractaccountmanager.h"

#include <QMap>

#include <Accounts/Account>

class KAccountsManager : public AbstractAccountManager
{
public:
    KAccountsManager();
    ~KAccountsManager() override;

    KGAPI2::AccountPtr account(const QString &accountName) override;
    KGAPI2::AccountPtr createAccount() override;
    KGAPI2::AccountPtr refreshAccount(const KGAPI2::AccountPtr &account) override;
    void removeAccount(const QString &accountName) override;
    QSet<QString> accounts() override;

private:
    void loadAccounts();

    KGAPI2::AccountPtr getAccountCredentials(Accounts::AccountId id, const QString &displayName);

    QMap<Accounts::AccountId, KGAPI2::AccountPtr> m_accounts;
};
