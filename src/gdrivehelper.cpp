/*
 * Copyright (C) 2014 Daniel Vrátil <dvratil@redhat.com>
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

#include "gdrivehelper.h"

#include <KIO/Job>
#include <KGAPI/Drive/File>
#include <KLocalizedString>

using namespace KGAPI2::Drive;

#define VND_GOOGLE_APPS_DOCUMENT        QStringLiteral("application/vnd.google-apps.document")
#define VND_GOOGLE_APPS_DRAWING         QStringLiteral("application/vnd.google-apps.drawing")
#define VND_GOOGLE_APPS_FILE            QStringLiteral("application/vnd.google-apps.file")
#define VND_GOOGLE_APPS_FORM            QStringLiteral("application/vnd.google-apps.form")
#define VND_GOOGLE_APPS_PRESENTATION    QStringLiteral("application/vnd.google-apps.presentation")
#define VND_GOOGLE_APPS_SPREADSHEET     QStringLiteral("application/vnd.google-apps.spreadsheet")
#define VND_GOOGLE_APPS_FOLDER          QStringLiteral("application/vnd.google-apps.folder")

#define VND_OASIS_OPENDOCUMENT_TEXT     QStringLiteral("application/vnd.oasis.opendocument.text")
#define VND_OASIS_OPENDOCUMENT_PRESENTATION QStringLiteral("application/vnd.oasis.opendocument.presentation")
// Google's Drive API v2 mistakenly documents an x-vnd style MIME type
// for .ods files, so we #define both the correct but undocumented,
// as well as the incorrect but publicly documented MIME type.
#define VND_OASIS_OPENDOCUMENT_SPREADSHEET QStringLiteral("application/vnd.oasis.opendocument.spreadsheet")
#define X_VND_OASIS_OPENDOCUMENT_SPREADSHEET QStringLiteral("application/x-vnd.oasis.opendocument.spreadsheet")


#define VND_OPENXMLFORMATS_OFFICEDOCUMENT_WORDPROCESSINGML_DOCUMENT \
            QStringLiteral("application/vnd.openxmlformats-officedocument.wordprocessingml.document")
#define VND_OPENXMLFORMATS_OFFICEDOCUMENT_PRESENTATIONML_PRESENTATION \
            QStringLiteral("application/vnd.openxmlformats-officedocument.presentationml.presentation")
#define VND_OPENXMLFORMATS_OFFICEDOCUMENT_SPREADSHEETML_SHEET \
            QStringLiteral("application/vnd.openxmlformats-officedocument.spreadsheetml.sheet")

#define IMAGE_PNG                       QStringLiteral("image/png")
#define IMAGE_JPEG                      QStringLiteral("image/jpeg")
#define APPLICATION_PDF                 QStringLiteral("application/pdf")

namespace GDriveHelper {

static const QMap<QString /* mimetype */, QString /* .ext */> ExtensionsMap{
    { VND_OASIS_OPENDOCUMENT_TEXT, QStringLiteral(".odt") },
    { VND_OASIS_OPENDOCUMENT_SPREADSHEET, QStringLiteral(".ods") },
    { VND_OASIS_OPENDOCUMENT_PRESENTATION, QStringLiteral(".odp") },
    { VND_OPENXMLFORMATS_OFFICEDOCUMENT_WORDPROCESSINGML_DOCUMENT, QStringLiteral(".docx") },
    { VND_OPENXMLFORMATS_OFFICEDOCUMENT_SPREADSHEETML_SHEET, QStringLiteral(".xlsx") },
    { VND_OPENXMLFORMATS_OFFICEDOCUMENT_PRESENTATIONML_PRESENTATION, QStringLiteral(".pptx") },
    { IMAGE_PNG, QStringLiteral(".png") },
    { IMAGE_JPEG, QStringLiteral(".jpg") },
    { APPLICATION_PDF, QStringLiteral(".pdf") }
};

static const QMap<QString /* mimetype */, QStringList /* target mimetypes */ > ConversionMap{
    { VND_GOOGLE_APPS_DOCUMENT, {
            VND_OASIS_OPENDOCUMENT_TEXT,
            VND_OPENXMLFORMATS_OFFICEDOCUMENT_WORDPROCESSINGML_DOCUMENT,
            APPLICATION_PDF }
    },
    { VND_GOOGLE_APPS_DRAWING, {
            IMAGE_PNG,
            IMAGE_JPEG,
            APPLICATION_PDF }
    },
    { VND_GOOGLE_APPS_PRESENTATION, {
            VND_OASIS_OPENDOCUMENT_PRESENTATION,
            VND_OPENXMLFORMATS_OFFICEDOCUMENT_PRESENTATIONML_PRESENTATION,
            APPLICATION_PDF }
    },
    { VND_GOOGLE_APPS_SPREADSHEET, {
            VND_OASIS_OPENDOCUMENT_SPREADSHEET,
            VND_OPENXMLFORMATS_OFFICEDOCUMENT_SPREADSHEETML_SHEET,
            APPLICATION_PDF }
    }
};

}

QString GDriveHelper::folderMimeType()
{
    return VND_GOOGLE_APPS_FOLDER;
}

bool GDriveHelper::isGDocsDocument(const KGAPI2::Drive::FilePtr &file)
{
    return GDriveHelper::ConversionMap.contains(file->mimeType());
}

QUrl GDriveHelper::convertFromGDocs(KGAPI2::Drive::FilePtr &file)
{
    const QString originalMimeType = file->mimeType();
    auto convIt = GDriveHelper::ConversionMap.constFind(originalMimeType);
    if (convIt == GDriveHelper::ConversionMap.cend()) {
        return file->downloadUrl();
    }

    const auto exportLinks = file->exportLinks();
    Q_FOREACH (const QString &targetMimeType, convIt.value()) {
        const auto linkIt = exportLinks.constFind(targetMimeType);
        if (linkIt != exportLinks.cend()) {
            // Extra check to safeguard against a mistake in Google's Drive API v2
            // documentation which lists an invalid MIME type for .ods files.
            if (targetMimeType == X_VND_OASIS_OPENDOCUMENT_SPREADSHEET) {
                file->setMimeType(VND_OASIS_OPENDOCUMENT_SPREADSHEET);
            } else {
                file->setMimeType(targetMimeType);
            }
            file->setTitle(file->title() + GDriveHelper::ExtensionsMap[targetMimeType]);
            return *linkIt;
        }
    }

    // If we got here that means that the file cannot be converted to any meaningful
    // type, so we do nothing
    return file->downloadUrl();
}

// Currently unused, see https://phabricator.kde.org/T3443
/*
KIO::UDSEntry GDriveHelper::trash()
{
    KIO::UDSEntry trashEntry;
    trashEntry.insert(KIO::UDSEntry::UDS_NAME, GDriveUrl::TrashDir);
    trashEntry.insert(KIO::UDSEntry::UDS_DISPLAY_NAME, i18n("Trash"));
    trashEntry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    trashEntry.insert(KIO::UDSEntry::UDS_ICON_NAME, QStringLiteral("user-trash"));
    return trashEntry;
}
*/

