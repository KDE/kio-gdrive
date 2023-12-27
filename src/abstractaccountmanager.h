/*
 * SPDX-FileCopyrightText: 2017 Elvis Angelaccio <elvis.angelaccio@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#pragma once

#include <QSet>

#include <KGAPI/Account>
#include <KIO/UDSEntry>

class AbstractAccountManager
{
public:
    virtual ~AbstractAccountManager();

    /**
     * @return Pointer to the account for @p accountName.
     * The account is valid only if @p accountName is in accounts().
     * @see accounts()
     */
    virtual KGAPI2::AccountPtr account(const QString &accountName) = 0;

    /**
     * Creates a new account.
     * @return The new account if a new account has been created, an invalid account otherwise.
     */
    virtual KGAPI2::AccountPtr createAccount() = 0;

    virtual KGAPI2::AccountPtr refreshAccount(const KGAPI2::AccountPtr &account) = 0;

    /**
     * Remove @p accountName from accounts().
     * @see accounts()
     */
    virtual void removeAccount(const QString &accountName) = 0;

    /**
     * @return The gdrive accounts managed by this object.
     */
    virtual QSet<QString> accounts() = 0;
};
