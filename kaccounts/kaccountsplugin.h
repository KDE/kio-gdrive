/*
 * SPDX-FileCopyrightText: 2017 Elvis Angelaccio <elvis.angelaccio@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#pragma once

#include <KAccounts/KAccountsDPlugin>

class GoogleDrivePlugin : public KAccounts::KAccountsDPlugin
{
    Q_OBJECT
public:
    explicit GoogleDrivePlugin(QObject *parent, const QVariantList &args);

public Q_SLOTS:
    void onAccountCreated(const Accounts::AccountId accountId, const Accounts::ServiceList &serviceList) override;
    void onAccountRemoved(const Accounts::AccountId accountId) override;
    void onServiceEnabled(const Accounts::AccountId accountId, const Accounts::Service &service) override;
    void onServiceDisabled(const Accounts::AccountId accountId, const Accounts::Service &service) override;
};
