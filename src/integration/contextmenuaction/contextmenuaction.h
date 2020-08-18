/*
 * Copyright (C) 2020 David Barchiesi <david@barchie.si>
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

#ifndef CONTEXTMENUACTION_H
#define CONTEXTMENUACTION_H

#include <KFileItem>
#include <KAbstractFileItemActionPlugin>


class ContextMenuAction : public KAbstractFileItemActionPlugin
{

    Q_OBJECT

public:
    ContextMenuAction(QObject *parent, const QVariantList &args);
    QList<QAction*> actions(const KFileItemListProperties &fileItemInfos, QWidget *parentWidget) override;

private:
    QAction *createCopyUrlAction(QWidget *parent, const QString &gdriveLink);
    QAction *createOpenUrlAction(QWidget *parent, const QString &gdriveLink);
};

#endif
