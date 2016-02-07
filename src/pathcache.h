/*
 * Copyright (C) 2014  Daniel Vrátil <dvratil@redhat.com>
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

#ifndef PATHCACHE_H
#define PATHCACHE_H

#include <QHash>
#include <QLoggingCategory>
#include <QStringList>

Q_DECLARE_LOGGING_CATEGORY(LOG_KIO_GDRIVE)

class PathCache
{
public:
    PathCache();
    ~PathCache();

    void insertPath(const QString &path, const QString &fileId);

    QString idForPath(const QString &path) const;
    QStringList descendants(const QString &path) const;
    void removePath(const QString &path);

    void dump();
private:
    QHash<QString /* path */, QString> m_pathIdMap;
};

#endif // PATHCACHE_H
