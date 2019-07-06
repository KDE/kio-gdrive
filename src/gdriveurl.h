/*
 * Copyright (C) 2014 Daniel Vrátil <dvratil@redhat.com>
 * Copyright (c) 2016 Elvis Angelaccio <elvis.angelaccio@kde.org>
 * Copyright (c) 2019 David Barchiesi <david@barchie.si>
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

#ifndef GDRIVEURL_U
#define GDRIVEURL_U

#include <QUrl>

class GDriveUrl
{
public:
    explicit GDriveUrl(const QUrl &url);

    QString account() const;
    QString filename() const;
    bool isRoot() const;
    bool isAccountRoot() const;
    bool isTopLevel() const;
    bool isSharedDrivesRoot() const;
    bool isSharedDrive() const;
    bool isTrashDir() const;
    bool isTrashed() const;
    QUrl url() const;
    QString parentPath() const;
    QStringList pathComponents() const;

    static const QString Scheme;
    static const QString SharedDrivesDir;
    static const QString TrashDir;
private:
    QUrl m_url;
    QStringList m_components;
};

#endif // GDRIVEURL_U
