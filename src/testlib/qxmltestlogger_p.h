/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtTest module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QXMLTESTLOGGER_P_H
#define QXMLTESTLOGGER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//


#include <QtTest/private/qabstracttestlogger_p.h>

QT_BEGIN_NAMESPACE

class QXmlTestLogger : public QAbstractTestLogger
{
public:
    enum XmlMode { Complete = 0, Light };

    QXmlTestLogger(XmlMode mode, const char *filename);
    ~QXmlTestLogger();

    void startLogging() override;
    void stopLogging() override;

    void enterTestFunction(const char *function) override;
    void leaveTestFunction() override;

    void addIncident(IncidentTypes type, const char *description,
                     const char *file = nullptr, int line = 0) override;
    void addBenchmarkResult(const QBenchmarkResult &result) override;

    void addMessage(MessageTypes type, const QString &message,
                    const char *file = nullptr, int line = 0) override;

    [[nodiscard]] static bool xmlCdata(QTestCharBuffer *dest, char const *src);
    [[nodiscard]] static bool xmlQuote(QTestCharBuffer *dest, char const *src);
private:
    [[nodiscard]] static int xmlCdata(QTestCharBuffer *dest, char const *src, qsizetype n);
    [[nodiscard]] static int xmlQuote(QTestCharBuffer *dest, char const *src, qsizetype n);

    XmlMode xmlmode;
};

QT_END_NAMESPACE

#endif
