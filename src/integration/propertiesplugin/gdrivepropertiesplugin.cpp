/*
 * SPDX-FileCopyrightText: 2020 David Barchiesi <david@barchie.si>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "gdrivepropertiesplugin.h"
#include "../../gdrive_udsentry.h"
#include "../../gdrivedebug.h"

#include <KIO/StatJob>
#include <KLocalizedString>
#include <KPluginFactory>
#include <QClipboard>
#include <QDesktopServices>
#include <QtGlobal>

K_PLUGIN_CLASS_WITH_JSON(GDrivePropertiesPlugin, "gdrivepropertiesplugin.json")

GDrivePropertiesPlugin::GDrivePropertiesPlugin(QObject *parent, const QList<QVariant> &args)
    : KPropertiesDialogPlugin(parent)
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
    const KIO::StatJob *job = KIO::stat(m_item.url(), KIO::HideProgressInfo);
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

    if (entry.contains(GDriveUDSEntryExtras::SharedWithMeDate)) {
        m_ui.sharedWithMeValue->setText(entry.stringValue(GDriveUDSEntryExtras::SharedWithMeDate));
        m_ui.sharedWithMeLabel->show();
        m_ui.sharedWithMeValue->show();
    } else {
        m_ui.sharedWithMeLabel->hide();
        m_ui.sharedWithMeValue->hide();
    }

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
    KIO::StatJob *statJob = qobject_cast<KIO::StatJob *>(job);
    if (!statJob || statJob->error()) {
        qCDebug(GDRIVE) << "Failed stat()ing" << statJob->url() << statJob->errorString();
        qCDebug(GDRIVE) << "Not showing Google Drive properties tab";
        return;
    }
    const KIO::UDSEntry entry = statJob->statResult();
    showEntryDetails(entry);
    properties->addPage(&m_widget, i18n("G&oogle Drive"));
}

#include "gdrivepropertiesplugin.moc"
