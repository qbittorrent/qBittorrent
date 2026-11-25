// SPDX-License-Identifier: GPL-2.0-or-later
#include "desktopstyle.h"

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QFontInfo>
#include <QPalette>

using namespace Qt::Literals::StringLiterals;

void DesktopStyle::apply()
{
    QFile sheet(u":/desktopstyle/widgets.qss"_s);
    if (!sheet.open(QIODevice::ReadOnly))
        return;

    QString rules = QString::fromUtf8(sheet.readAll());

    const QFont font = qApp->font();
    const QString sectionSize = (font.pixelSize() > 0)
        ? u"%1px"_s.arg(qRound(font.pixelSize() * 1.15))
        : u"%1pt"_s.arg(QFontInfo(font).pointSizeF() * 1.15);
    const QPalette palette = qApp->palette();
    const int base = qGray(palette.base().color().rgb());
    const int text = qGray(palette.text().color().rgb());
    const auto shade = [base, text](const int percent)
    {
        const int gray = base + qRound((text - base) * percent / 100.0);
        return QColor(gray, gray, gray).name();
    };
    qApp->setStyleSheet(rules
        .replace(u"@sectionFontSize@"_s, sectionSize)
        .replace(u"@inkSuffix@"_s, (base < text) ? u"-inverse"_s : QString())
        .replace(u"@selection@"_s, shade(15))
        .replace(u"@hover@"_s, shade(8))
        .replace(u"@alternate@"_s, shade(4)));
}
