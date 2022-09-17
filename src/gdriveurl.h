/*
 * SPDX-FileCopyrightText: 2014 Daniel Vrátil <dvratil@redhat.com>
 * SPDX-FileCopyrightText: 2016 Elvis Angelaccio <elvis.angelaccio@kde.org>
 * SPDX-FileCopyrightText: 2019 David Barchiesi <david@barchie.si>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
    bool isNewAccountPath() const;
    bool isTopLevel() const;
    bool isSharedWithMeRoot() const;
    bool isSharedWithMeTopLevel() const;
    bool isSharedWithMe() const;
    bool isSharedDrivesRoot() const;
    bool isSharedDrive() const;
    bool isTrashDir() const;
    bool isTrashed() const;
    QUrl url() const;
    QString parentPath() const;
    QStringList pathComponents() const;

    static QString buildSharedDrivePath(const QString &accountId, const QString &drive);

    static const QString Scheme;
    static const QString SharedWithMeDir;
    static const QString SharedDrivesDir;
    static const QString TrashDir;
    static const QString NewAccountPath;

private:
    QUrl m_url;
    QStringList m_components;
};

#endif // GDRIVEURL_U
