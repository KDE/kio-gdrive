/*
 * Copyright 2020  David Barchiesi <david@barchie.si>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GDRIVEPROPERTIESPLUGIN_H
#define GDRIVEPROPERTIESPLUGIN_H

#include <KPropertiesDialog>

#include "ui_gdrivepropertiesplugin.h"

class GDrivePropertiesPlugin : public KPropertiesDialogPlugin
{
    Q_OBJECT
public:
    explicit GDrivePropertiesPlugin(QObject *parent, const QList<QVariant> &args);
    ~GDrivePropertiesPlugin() override = default;

private:
    QWidget m_widget;
    Ui::GDrivePropertiesWidget m_ui;
    KFileItem m_item;

    void showEntryDetails(const KIO::UDSEntry &entry);

private slots:
    void statJobFinished(KJob *job);
};

#endif // GDRIVEPROPERTIESPLUGIN_H
