/*
 * Copyright (C) 2019 David Barchiesi <david@barchie.si>
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

#include "copyurlitemaction.h"
#include "../../gdrive_udsentry.h"

#include <QGuiApplication>
#include <QClipboard>
#include <QAction>

#include <KFileItemListProperties>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KFileItem>


K_PLUGIN_CLASS_WITH_JSON(CopyUrlItemAction, "copyurlitemaction.json")

CopyUrlItemAction::CopyUrlItemAction(QObject* parent, const QVariantList&)
    : KAbstractFileItemActionPlugin(parent)
{}

QList<QAction*> CopyUrlItemAction::actions(const KFileItemListProperties& fileItemInfos, QWidget* parentWidget)
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

    QAction *copyUrlAction = createCopyUrlAction(parentWidget, gdriveLink);
    return { copyUrlAction };
}


QAction *CopyUrlItemAction::createCopyUrlAction(QWidget *parent, const QString& gdriveLink)
{
    const QString name = i18n("Copy Google URL to clipboard");
    const QIcon icon = QIcon::fromTheme(QStringLiteral("folder-gdrive"));
    QAction *action = new QAction(icon, name, parent);

    connect(action, &QAction::triggered, this, [gdriveLink]() {
            QGuiApplication::clipboard()->setText(gdriveLink);
            });

    return action;
}


#include "copyurlitemaction.moc"
