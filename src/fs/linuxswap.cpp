/*
    SPDX-FileCopyrightText: 2008-2010 Volker Lanz <vl@fidra.de>
    SPDX-FileCopyrightText: 2012-2018 Andrius Štikonas <andrius@stikonas.eu>
    SPDX-FileCopyrightText: 2016 Teo Mrnjavac <teo@kde.org>
    SPDX-FileCopyrightText: 2016 Chantara Tith <tith.chantara@gmail.com>
    SPDX-FileCopyrightText: 2019 Yuri Chornoivan <yurchor@ukr.net>
    SPDX-FileCopyrightText: 2020 Arnaud Ferraris <arnaud.ferraris@collabora.com>
    SPDX-FileCopyrightText: 2020 Gaël PORTAY <gael.portay@collabora.com>

    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "fs/linuxswap.h"

#include "util/externalcommand.h"

#include <KLocalizedString>

#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

namespace FS
{
FileSystem::CommandSupportType linuxswap::m_Create = FileSystem::cmdSupportNone;
FileSystem::CommandSupportType linuxswap::m_Grow = FileSystem::cmdSupportNone;
FileSystem::CommandSupportType linuxswap::m_Shrink = FileSystem::cmdSupportNone;
FileSystem::CommandSupportType linuxswap::m_Move = FileSystem::cmdSupportNone;
FileSystem::CommandSupportType linuxswap::m_Copy = FileSystem::cmdSupportNone;
FileSystem::CommandSupportType linuxswap::m_GetUsed = FileSystem::cmdSupportNone;
FileSystem::CommandSupportType linuxswap::m_GetLabel = FileSystem::cmdSupportNone;
FileSystem::CommandSupportType linuxswap::m_SetLabel = FileSystem::cmdSupportNone;
FileSystem::CommandSupportType linuxswap::m_GetUUID = FileSystem::cmdSupportNone;
FileSystem::CommandSupportType linuxswap::m_UpdateUUID = FileSystem::cmdSupportNone;

linuxswap::linuxswap(qint64 firstsector, qint64 lastsector, qint64 sectorsused, const QString& label, const QVariantMap& features) :
    FileSystem(firstsector, lastsector, sectorsused, label, features, FileSystem::Type::LinuxSwap)
{
}

void linuxswap::init()
{
    m_SetLabel = m_Shrink = m_Grow = m_Create = m_UpdateUUID = (findExternal(QStringLiteral("mkswap"))) ? cmdSupportFileSystem : cmdSupportNone;
    m_GetLabel = cmdSupportCore;
    m_GetUsed = cmdSupportFileSystem;
    m_Copy = cmdSupportFileSystem;
    m_Move = cmdSupportCore;
    m_GetUUID = cmdSupportCore;
}

bool linuxswap::supportToolFound() const
{
    return
        m_GetUsed != cmdSupportNone &&
        m_GetLabel != cmdSupportNone &&
        m_SetLabel != cmdSupportNone &&
        m_Create != cmdSupportNone &&
//         m_Check != cmdSupportNone &&
        m_UpdateUUID != cmdSupportNone &&
        m_Grow != cmdSupportNone &&
        m_Shrink != cmdSupportNone &&
        m_Copy != cmdSupportNone &&
        m_Move != cmdSupportNone &&
//         m_Backup != cmdSupportNone &&
        m_GetUUID != cmdSupportNone;
}

FileSystem::SupportTool linuxswap::supportToolName() const
{
    return SupportTool(QStringLiteral("util-linux"), QUrl(QStringLiteral("https://github.com/karelzak/util-linux")));
}

int linuxswap::maxLabelLength() const
{
    return 15;
}

bool linuxswap::create(Report& report, const QString& deviceNode)
{
    ExternalCommand cmd(report, QStringLiteral("mkswap"), { deviceNode });
    return cmd.run(-1) && cmd.exitCode() == 0;
}

bool linuxswap::resize(Report& report, const QString& deviceNode, qint64 length) const
{
    Q_UNUSED(length)
    const QString label = readLabel(deviceNode);
    const QString uuid = readUUID(deviceNode);

    QStringList args;
    if (!label.isEmpty())
        args << QStringLiteral("--label") << label;
    if (!uuid.isEmpty())
        args << QStringLiteral("--uuid") << uuid;

    args << deviceNode;

    ExternalCommand cmd(report, QStringLiteral("mkswap"), args);
    return cmd.run(-1) && cmd.exitCode() == 0;
}

bool linuxswap::copy(Report& report, const QString& targetDeviceNode, const QString& sourceDeviceNode) const
{
    const QString label = readLabel(sourceDeviceNode);
    const QString uuid = readUUID(sourceDeviceNode);

    QStringList args;
    if (!label.isEmpty())
        args << QStringLiteral("--label") << label;
    if (!uuid.isEmpty())
        args << QStringLiteral("--uuid") << uuid;

    args << targetDeviceNode;

    ExternalCommand cmd(report, QStringLiteral("mkswap"), args);
    return cmd.run(-1) && cmd.exitCode() == 0;
}

bool linuxswap::writeLabel(Report& report, const QString& deviceNode, const QString& newLabel)
{
    ExternalCommand cmd(report, QStringLiteral("swaplabel"), { QStringLiteral("--label"), newLabel, deviceNode });
    return cmd.run(-1) && cmd.exitCode() == 0;
}

QString linuxswap::mountTitle() const
{
    return xi18nc("@title:menu", "Activate swap");
}

QString linuxswap::unmountTitle() const
{
    return xi18nc("@title:menu", "Deactivate swap");
}

bool linuxswap::canMount(const QString& deviceNode, const QString& mountPoint) const {
    Q_UNUSED(deviceNode)
    // linux swap doesn't require mount point to activate
    return mountPoint != QStringLiteral("/");
}

bool linuxswap::mount(Report& report, const QString& deviceNode, const QString& mountPoint)
{
    Q_UNUSED(mountPoint)
    ExternalCommand cmd(report, QStringLiteral("swapon"), { deviceNode });
    return cmd.run(-1) && cmd.exitCode() == 0;
}

bool linuxswap::unmount(Report& report, const QString& deviceNode)
{
    ExternalCommand cmd(report, QStringLiteral("swapoff"), { deviceNode });
    return cmd.run(-1) && cmd.exitCode() == 0;
}

bool linuxswap::updateUUID(Report& report, const QString& deviceNode) const
{
    const QString label = readLabel(deviceNode);

    QStringList args;
    if (!label.isEmpty())
        args << QStringLiteral("--label") << label;

    args << deviceNode;

    ExternalCommand cmd(report, QStringLiteral("mkswap"), args);
    return cmd.run(-1) && cmd.exitCode() == 0;
}

qint64 linuxswap::readUsedCapacity(const QString& deviceNode) const
{
    QFile swapsFile(QStringLiteral("/proc/swaps"));

    if (swapsFile.open(QIODevice::ReadOnly)) {
        QByteArray data = swapsFile.readAll();
        swapsFile.close();
        QTextStream in(&data);
        while (!in.atEnd()) {
            QStringList line = in.readLine().split(QRegularExpression(QStringLiteral("\\s+")));
            QFileInfo kernelPath(deviceNode);
            if (line[0] == kernelPath.canonicalFilePath())
                return line[3].toLongLong() * 1024;
        }
    }
    return -1;
}
}
