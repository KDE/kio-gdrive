/*
 * SPDX-FileCopyrightText: 2014 Daniel Vrátil <dvratil@redhat.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#ifndef GDRIVEHELPER_H
#define GDRIVEHELPER_H

#include <KGAPI/Types>
#include <KIO/UDSEntry>

namespace GDriveHelper
{
QString folderMimeType();

bool isGDocsDocument(const KGAPI2::Drive::FilePtr &file);

QUrl convertFromGDocs(KGAPI2::Drive::FilePtr &file);

QUrl downloadUrl(const KGAPI2::Drive::FilePtr &file);

KIO::UDSEntry trash();

QString elideToken(const QString &token);
}

#endif // GDRIVEHELPER_H
