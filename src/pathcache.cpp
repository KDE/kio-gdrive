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

PathCache::Record::Record()
    : timestamp(-1)
{
}

PathCache::Record::Record(const PathCache::Record &other)
    : value(other.value)
    , timestamp(other.timestamp)
{
}

PathCache::Record::Record(const QString &value, const QString &parentId)
    : value(value)
    , ancestor(parentId)
    , timestamp(QDateTime::currentMSecsSinceEpoch())
{
}

PathCache::PathCache(int expireSeconds)
    : m_expireSeconds(expireSeconds)
{
}

PathCache::~PathCache()
{
}

bool PathCache::isRecordExpired(const PathCache::Record& record)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    Q_ASSERT(record.timestamp <= now); // records must originate in the past

    return ((now - record.timestamp) >= m_expireSeconds);
}

QString PathCache::valueForKey(QHash<QString, PathCache::Record> &map, const QString &key)
{
    if (!map.contains(key)) {
        return QString();
    }

    const Record record = map.value(key);
    if (isRecordExpired(record)) {
        map.remove(key);
        return QString();
    }

    return record.value;
}


void PathCache::insertPath(const QString &id, const QString &name, const QString &parentId)
{
    m_idNameMap.insert(id, Record(name, parentId));
    m_nameIdMap.insert(name, Record(id, parentId));
    m_parentChildrenMap.insert(parentId, Record(id, QString()));
}

QString PathCache::idForName(const QString &name)
{
    return valueForKey(m_nameIdMap, name);
}

QString PathCache::nameForId(const QString &id)
{
    return valueForKey(m_idNameMap, id);
}

QStringList PathCache::descendants(const QString &parentId)
{
    if (!m_parentChildrenMap.contains(parentId)) {
        return QStringList();
    }

    QHash<QString, Record>::Iterator it = m_parentChildrenMap.find(parentId);
    bool expired = false;
    QStringList ids;
    while (it != m_parentChildrenMap.end()) {
        if (isRecordExpired(*it)) {
            it = m_parentChildrenMap.erase(it);
            // The idea is that when the list contains at least one expired item,
            // we continue scanning to find other expired entries and we return empty
            // list to force cache update (even if there are non-expired entries)
            ids.clear();
            expired = true;
            continue;
        }

        if (!expired) {
            ids.append(it->value);
        }
        ++it;
    }

    return ids;
}
