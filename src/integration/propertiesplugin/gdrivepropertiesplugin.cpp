/*
 * Copyright 2020  David Barchiesi <david@barchie.si>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gdrivepropertiesplugin.h"
#include "../../gdrivedebug.h"
#include "../../gdrive_udsentry.h"

#include <KPluginFactory>
#include <KPluginLoader>
#include <QClipboard>
#include <QDesktopServices>
#include <KIO/StatJob>

K_PLUGIN_FACTORY(GDrivePropertiesPluginFactory, registerPlugin<GDrivePropertiesPlugin>();)

GDrivePropertiesPlugin::GDrivePropertiesPlugin(QObject *parent, const QList<QVariant> &args)
: KPropertiesDialogPlugin(qobject_cast<KPropertiesDialog *>(parent))
{
    Q_UNUSED(args)

    qCDebug(GDRIVE) << "Starting Google Drive properties tab";

    // Ignore if more than one file is selected
    if (properties->items().size() != 1) {
        qCDebug(GDRIVE) << "Can't show Google Drive properties tab for more than one item";
        return;
    }

    m_item = properties->items().at(0);

    // Ignore if not a Google Drive url
    if (m_item.url().scheme() != QLatin1String("gdrive")) {
        qCDebug(GDRIVE) << "Can't show Google Drive properties for non Google Drive entries";
        return;
    }

    m_ui.setupUi(&m_widget);

    // Re stat() the item because the entry is probably lacking required information.
    const KIO::StatJob* job = KIO::stat(m_item.url(), KIO::HideProgressInfo);
    connect(job, &KJob::finished, this, &GDrivePropertiesPlugin::statJobFinished);
}

void GDrivePropertiesPlugin::showEntryDetails(const KIO::UDSEntry &entry)
{
    const QString id = entry.stringValue(GDriveUDSEntryExtras::Id);
    m_ui.idValue->setText(id);

    const QString created = m_item.timeString(KFileItem::CreationTime);
    m_ui.createdValue->setText(created);

    const QString modified = m_item.timeString(KFileItem::ModificationTime);
    m_ui.modifiedValue->setText(modified);

    const QString lastViewedByMe = m_item.timeString(KFileItem::AccessTime);
    m_ui.lastViewedByMeValue->setText(lastViewedByMe);

    const QString version = entry.stringValue(GDriveUDSEntryExtras::Version);
    m_ui.versionValue->setText(version);

    const QString md5 = entry.stringValue(GDriveUDSEntryExtras::Md5);
    m_ui.md5Value->setText(md5);

    const QString lastModifyingUserName = entry.stringValue(GDriveUDSEntryExtras::LastModifyingUser);
    m_ui.lastModifiedByValue->setText(lastModifyingUserName);

    const QString owners = entry.stringValue(GDriveUDSEntryExtras::Owners);
    m_ui.ownersValue->setText(owners);

    const QString description = entry.stringValue(KIO::UDSEntry::UDS_COMMENT);
    m_ui.descriptionValue->setText(description);

    const QString gdriveLink = entry.stringValue(GDriveUDSEntryExtras::Url);
    connect(m_ui.urlOpenButton, &QPushButton::clicked, this, [gdriveLink]() {
        QDesktopServices::openUrl(QUrl(gdriveLink));
    });

    connect(m_ui.urlCopyButton, &QPushButton::clicked, this, [gdriveLink]() {
        QGuiApplication::clipboard()->setText(gdriveLink);
    });
}

void GDrivePropertiesPlugin::statJobFinished(KJob *job)
{
    KIO::StatJob *statJob = qobject_cast<KIO::StatJob*>(job);
    if (!statJob || statJob->error()) {
        qCDebug(GDRIVE) << "Failed stat()ing" << statJob->url() << statJob->errorString();
        qCDebug(GDRIVE) << "Not showing Google Drive properties tab";
        return;
    }
    const KIO::UDSEntry entry = statJob->statResult();
    showEntryDetails(entry);
    properties->addPage(&m_widget, "G&oogle Drive");
}

#include "gdrivepropertiesplugin.moc"
