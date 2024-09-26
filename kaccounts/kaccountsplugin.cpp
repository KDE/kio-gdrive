/*
 * SPDX-FileCopyrightText: 2017 Elvis Angelaccio <elvis.angelaccio@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#include "kaccountsplugin.h"

#include <KAccounts/Core>

#include <KIO/OpenUrlJob>
#include <KLocalizedString>
#include <KNotification>

#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(GoogleDrivePlugin, "kaccountsplugin.json")

GoogleDrivePlugin::GoogleDrivePlugin(QObject *parent, const QVariantList &args)
    : KAccounts::KAccountsDPlugin(parent, args)
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
    notification->setText(
        xi18nc("@info", "You can now manage the Google Drive files of your <emphasis strong='true'>%1</emphasis> account.", account->displayName()));

    QUrl url;
    url.setScheme(QStringLiteral("gdrive"));
    url.setPath(QStringLiteral("/%1").arg(account->displayName()));

    auto action = notification->addAction(i18n("Open"));
    connect(action, &KNotificationAction::activated, this, [url] {

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
