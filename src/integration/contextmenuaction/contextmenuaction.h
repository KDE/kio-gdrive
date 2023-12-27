/*
 * SPDX-FileCopyrightText: 2020 David Barchiesi <david@barchie.si>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#ifndef CONTEXTMENUACTION_H
#define CONTEXTMENUACTION_H

#include <KAbstractFileItemActionPlugin>
#include <KFileItem>

class ContextMenuAction : public KAbstractFileItemActionPlugin
{
    Q_OBJECT

public:
    ContextMenuAction(QObject *parent, const QVariantList &args);
    QList<QAction *> actions(const KFileItemListProperties &fileItemInfos, QWidget *parentWidget) override;

private:
    QAction *createCopyUrlAction(QWidget *parent, const QString &gdriveLink);
    QAction *createOpenUrlAction(QWidget *parent, const QString &gdriveLink);
};

#endif
