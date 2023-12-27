/*
 * SPDX-FileCopyrightText: 2020 David Barchiesi <david@barchie.si>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#include "contextmenuaction.h"
#include "../../gdrive_udsentry.h"

#include <QAction>
#include <QClipboard>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QMenu>

#include <KFileItemListProperties>
#include <KLocalizedString>
#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(ContextMenuAction, "contextmenuaction.json")

ContextMenuAction::ContextMenuAction(QObject *parent, const QVariantList &)
    : KAbstractFileItemActionPlugin(parent)
{
}

QList<QAction *> ContextMenuAction::actions(const KFileItemListProperties &fileItemInfos, QWidget *parentWidget)
{
    // Ignore if more than one file is selected
    if (fileItemInfos.items().size() != 1) {
        return {};
    }

    const KFileItem item = fileItemInfos.items().at(0);

    // Ignore if not a Google Drive url
    if (item.url().scheme() != QLatin1String("gdrive")) {
        return {};
    }

    const KIO::UDSEntry entry = item.entry();
    const QString gdriveLink = entry.stringValue(GDriveUDSEntryExtras::Url);
    // Ignore if missing a shareable link
    if (gdriveLink.isEmpty()) {
        return {};
    }

    QMenu *gdriveMenu = new QMenu(parentWidget);
    gdriveMenu->addAction(createOpenUrlAction(parentWidget, gdriveLink));
    gdriveMenu->addAction(createCopyUrlAction(parentWidget, gdriveLink));

    QAction *gdriveMenuAction = new QAction(i18n("Google Drive"), parentWidget);
    gdriveMenuAction->setMenu(gdriveMenu);
    gdriveMenuAction->setIcon(QIcon::fromTheme(QStringLiteral("folder-gdrive")));

    return {gdriveMenuAction};
}

QAction *ContextMenuAction::createCopyUrlAction(QWidget *parent, const QString &gdriveLink)
{
    const QString name = i18n("Copy URL to clipboard");
    const QIcon icon = QIcon::fromTheme(QStringLiteral("edit-copy"));
    QAction *action = new QAction(icon, name, parent);

    connect(action, &QAction::triggered, this, [gdriveLink]() {
        QGuiApplication::clipboard()->setText(gdriveLink);
    });

    return action;
}

QAction *ContextMenuAction::createOpenUrlAction(QWidget *parent, const QString &gdriveLink)
{
    const QString name = i18n("Open in browser");
    const QIcon icon = QIcon::fromTheme(QStringLiteral("internet-services"));
    QAction *action = new QAction(icon, name, parent);

    connect(action, &QAction::triggered, this, [gdriveLink]() {
        QDesktopServices::openUrl(QUrl(gdriveLink));
    });

    return action;
}

#include "contextmenuaction.moc"
