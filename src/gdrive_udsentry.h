/*
 * SPDX-FileCopyrightText: 2020 David Barchiesi <david@barchie.si>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#ifndef GDRIVEUDSENTRY_H
#define GDRIVEUDSENTRY_H

#include <KIO/UDSEntry>

enum GDriveUDSEntryExtras {
    Url = KIO::UDSEntry::UDS_EXTRA,
    Id,
    Md5,
    Owners,
    Version,
    LastModifyingUser,
    Description,
    SharedWithMeDate,
};

#endif // GDRIVEUDSENTRY_H
