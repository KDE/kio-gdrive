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

#include "pathcache.h"
#include <QDateTime>

#include <KDebug>

PathCache::PathCache()
{
}

PathCache::~PathCache()
{
}

void PathCache::insertPath(const QString &path, const QString &fileId)
{
    kDebug() << path;
    if (path.startsWith(QLatin1Char('/'))) {
        m_pathIdMap.insert(path.mid(1), fileId);
    } else {
        m_pathIdMap.insert(path, fileId);
    }
}

QString PathCache::idForPath(const QString &path) const
{
    if (path.startsWith(QLatin1Char('/'))) {
        return m_pathIdMap[path.mid(1)];
    } else {
        return m_pathIdMap[path];
    }
}

QStringList PathCache::descendants(const QString &path) const
{
    const QString fullPath = path.endsWith(QLatin1Char('/')) ? path : path + QLatin1Char('/');

    QStringList descendants;
    for (auto iter = m_pathIdMap.begin(); iter != m_pathIdMap.end(); ++iter) {
        if (!iter.key().startsWith(fullPath)) {
            // Not a descendant at all
            continue;
        }

        if (iter.key().lastIndexOf(QLatin1Char('/')) >= fullPath.size()) {
            // Not a direct descendat
            continue;
        }

        descendants.append(iter.key());
    }

    return descendants;
}

void PathCache::dump()
{
    kDebug() << "==== DUMP ====";
    for (auto iter = m_pathIdMap.constBegin(); iter != m_pathIdMap.constEnd(); ++iter) {
        kDebug() << iter.key() << " => " << iter.value();
    }
    kDebug() << "==== DUMP ====";
}

