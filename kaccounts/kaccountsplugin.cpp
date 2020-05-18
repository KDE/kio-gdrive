/*
 * Copyright (c) 2017 Elvis Angelaccio <elvis.angelaccio@kde.org>
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

#include "kaccountsplugin.h"

#include <KAccounts/Core>

#include <KLocalizedString>
#include <KNotification>
#include <KIO/OpenUrlJob>

#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(GoogleDrivePlugin, "kaccountsplugin.json")

GoogleDrivePlugin::GoogleDrivePlugin(QObject *parent, const QVariantList &args)
    : KAccountsDPlugin(parent, args)
{
}

void GoogleDrivePlugin::onAccountCreated(const Accounts::AccountId accountId, const Accounts::ServiceList &serviceList)
{
    Q_UNUSED(serviceList)
    auto account = Accounts::Account::fromId(KAccounts::accountsManager(), accountId);

    if (account->providerName() != QLatin1String("google")) {
        return;
    }

    auto notification = new KNotification(QStringLiteral("new-account-added"));
    notification->setComponentName(QStringLiteral("gdrive"));
    notification->setTitle(i18n("New Online Account"));
    notification->setText(xi18nc("@info", "You can now manage the Google Drive files of your <emphasis strong='true'>%1</emphasis> account.", account->displayName()));
    notification->setActions({i18n("Open")});

    QUrl url;
    url.setScheme(QStringLiteral("gdrive"));
    url.setPath(QStringLiteral("/%1").arg(account->displayName()));

    connect(notification, static_cast<void (KNotification::*)(unsigned int)>(&KNotification::activated), this, [=]() {
        KIO::OpenUrlJob *job = new KIO::OpenUrlJob(url, QStringLiteral("inode/directory"));
        job->start();
    });

    notification->sendEvent();
}

void GoogleDrivePlugin::onAccountRemoved(const Accounts::AccountId accountId)
{
    Q_UNUSED(accountId)
}

void GoogleDrivePlugin::onServiceEnabled(const Accounts::AccountId accountId, const Accounts::Service &service)
{
    Q_UNUSED(accountId)
    Q_UNUSED(service)
}

void GoogleDrivePlugin::onServiceDisabled(const Accounts::AccountId accountId, const Accounts::Service &service)
{
    Q_UNUSED(accountId)
    Q_UNUSED(service)
}

#include "kaccountsplugin.moc"
