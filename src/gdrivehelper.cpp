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

#include <LibKGAPI2/Drive/File>

using namespace KGAPI2::Drive;

#define VND_GOOGLE_APPS_DOCUMENT        QLatin1String("application/vnd.google-apps.document")
#define VND_GOOGLE_APPS_DRAWING         QLatin1String("application/vnd.google-apps.drawing")
#define VND_GOOGLE_APPS_FILE            QLatin1String("application/vnd.google-apps.file")
#define VND_GOOGLE_APPS_FORM            QLatin1String("application/vnd.google-apps.form")
#define VND_GOOGLE_APPS_PRESENTATION    QLatin1String("application/vnd.google-apps.presentation")
#define VND_GOOGLE_APPS_SPREADSHEET     QLatin1String("application/vnd.google-apps.spreadsheet")
#define VND_GOOGLE_APPS_FOLDER          QLatin1String("application/vnd.google-apps.folder")

#define VND_OASIS_OPENDOCUMENT_TEXT     QLatin1String("application/vnd.oasis.opendocument.text")
#define VND_OASIS_OPENDOCUMENT_PRESENTATION QLatin1String("application/vnd.oasis.opendocument.presentation")
#define VND_OASIS_OPENDOCUMENT_SPREADSHEED QLatin1String("application/x-vnd.oasis.opendocument.spreadsheet")

#define VND_OPENXMLFORMATS_OFFICEDOCUMENT_WORDPROCESSINGML_DOCUMENT \
            QLatin1String("application/vnd.openxmlformats-officedocument.wordprocessingml.document")
#define VND_OPENXMLFORMATS_OFFICEDOCUMENT_PRESENTATIONML_PRESENTATION \
            QLatin1String("application/vnd.openxmlformats-officedocument.presentationml.presentation")
#define VND_OPENXMLFORMATS_OFFICEDOCUMENT_SPREADSHEETML_SHEET \
            QLatin1String("application/vnd.openxmlformats-officedocument.spreadsheetml.sheet")

#define IMAGE_PNG                       QLatin1String("image/png")
#define IMAGE_JPEG                      QLatin1String("image/jpeg")
#define APPLICATION_PDF                 QLatin1String("application/pdf")

namespace GDriveHelper {

static const QMap<QString /* mimetype */, QString /* .ext */> ExtensionsMap = []() {
    QMap<QString, QString> ext;
    ext[VND_OASIS_OPENDOCUMENT_TEXT] = QLatin1String(".odt");
    ext[VND_OASIS_OPENDOCUMENT_SPREADSHEED] = QLatin1String(".ods");
    ext[VND_OASIS_OPENDOCUMENT_PRESENTATION] = QLatin1String(".odp");
    ext[VND_OPENXMLFORMATS_OFFICEDOCUMENT_WORDPROCESSINGML_DOCUMENT] = QLatin1String(".docx");
    ext[VND_OPENXMLFORMATS_OFFICEDOCUMENT_SPREADSHEETML_SHEET] = QLatin1String(".xlsx");
    ext[VND_OPENXMLFORMATS_OFFICEDOCUMENT_PRESENTATIONML_PRESENTATION] = QLatin1String(".pptx");
    ext[IMAGE_PNG] = QLatin1String(".png");
    ext[IMAGE_JPEG] = QLatin1String(".jpg");
    ext[APPLICATION_PDF] = QLatin1String(".pdf");
    return ext;
}();

static const QMap<QString /* mimetype */, QStringList /* target mimetypes */ > ConversionMap = []() {
    QMap<QString, QStringList> map;

    map[VND_GOOGLE_APPS_DOCUMENT] = {
            VND_OASIS_OPENDOCUMENT_TEXT,
            VND_OPENXMLFORMATS_OFFICEDOCUMENT_WORDPROCESSINGML_DOCUMENT,
            APPLICATION_PDF
    };

    map[VND_GOOGLE_APPS_DRAWING] = {
            IMAGE_PNG,
            IMAGE_JPEG,
            APPLICATION_PDF
    };

    map[VND_GOOGLE_APPS_PRESENTATION] = {
            VND_OASIS_OPENDOCUMENT_PRESENTATION,
            VND_OPENXMLFORMATS_OFFICEDOCUMENT_PRESENTATIONML_PRESENTATION,
            APPLICATION_PDF
    };

    map[VND_GOOGLE_APPS_SPREADSHEET] = {
            VND_OASIS_OPENDOCUMENT_SPREADSHEED,
            VND_OPENXMLFORMATS_OFFICEDOCUMENT_SPREADSHEETML_SHEET,
            APPLICATION_PDF
    };

    return map;
}();

}

QString GDriveHelper::folderMimeType() const
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
    if (!GDriveHelper::ConversionMap.contains(originalMimeType)) {
        return file->downloadUrl();
    }

    const auto exportLinks = file->exportLinks();
    for (const QString &targetMimeType : GDriveHelper::ConversionMap[originalMimeType]) {
        if (exportLinks.contains(targetMimeType)) {
            file->setMimeType(targetMimeType);
            file->setTitle(file->title() + GDriveHelper::ExtensionsMap[targetMimeType]);
            return exportLinks[targetMimeType];
        }
    }

    // If we got here that means that the file cannot be converted to any meaningful
    // type, so we do nothing
    return file->downloadUrl();
}


