/*
    SPDX-FileCopyrightText: 2024 Colin MacLean <colin@colin-maclean.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef HIERARCHICAL_SESSION_H
#define HIERARCHICAL_SESSION_H

#include "Session.h"

namespace Konsole
{

class KONSOLEPRIVATE_EXPORT HierarchicalSession : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.konsole.HierarchicalSession")

public:
    using Ptr = QPointer<HierarchicalSession>;

    Q_PROPERTY(QString name READ nameTitle)
    Q_PROPERTY(int processId READ processId)
    Q_PROPERTY(QString keyBindings READ keyBindings WRITE setKeyBindings)
    Q_PROPERTY(QSize size READ size WRITE setSize)

    explicit HierarchicalSession(QObject *parent = nullptr);
    ~HierarchicalSession() override;
};

}

#endif
