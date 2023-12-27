/*
    SPDX-FileCopyrightText: 2020 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "gdrivejob.h"

#include <KIO/CopyJob>

QList<QUrl> arrayToList(const QJsonArray &array)
{
    QList<QUrl> ret;
    for (const QJsonValue &val : array) {
        ret += val.toVariant().toUrl();
    }
    return ret;
}

void GDriveJob::start()
{
    const QString accountName = data().value(QStringLiteral("accountName")).toString();
    QString folder = data().value(QStringLiteral("folder")).toString();

    if (!folder.startsWith(QLatin1Char('/'))) {
        folder.prepend(QLatin1Char('/'));
    }

    QUrl destUrl;
    destUrl.setScheme(QStringLiteral("gdrive"));
    destUrl.setPath(accountName + folder);

    const QList<QUrl> sourceUrls = arrayToList(data().value(QStringLiteral("urls")).toArray());

    KIO::CopyJob *copyJob = KIO::copy(sourceUrls, destUrl);

    connect(copyJob, &KIO::CopyJob::finished, this, [this, copyJob] {
        if (copyJob->error()) {
            setError(copyJob->error());
            setErrorText(copyJob->errorText());
        }
        emitResult();
    });

    copyJob->start();
}
