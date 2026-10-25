/*
  Q Light Controller Plus - Unit test
  theme_test.cpp

  Copyright (C) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QSet>
#include <QVBoxLayout>
#include <QWidget>
#include <QtTest>

#include "theme_test.h"
#include "appicons.h"
#include "uieffects.h"
#include "theme.h"
#include "appcard.h"
#include "appswitch.h"
#include "appsegmentedcontrol.h"
#include "appdialog.h"
#include "appemptystate.h"
#include "appsettingrow.h"

void Theme_Test::initTestCase()
{
    m_originalPalette = QApplication::palette();
}

void Theme_Test::schemeStrings()
{
    QCOMPARE(Theme::schemeToString(Theme::OLED), QString("oled"));
    QCOMPARE(Theme::schemeToString(Theme::Dark), QString("midnight"));
    QCOMPARE(Theme::schemeToString(Theme::Light), QString("light"));
    QCOMPARE(Theme::schemeToString(Theme::Classic), QString("classic"));

    QCOMPARE(Theme::schemeFromString("oled"), Theme::OLED);
    QCOMPARE(Theme::schemeFromString("MIDNIGHT"), Theme::Dark);
    QCOMPARE(Theme::schemeFromString("dark"), Theme::Dark);
    QCOMPARE(Theme::schemeFromString("light"), Theme::Light);
    QCOMPARE(Theme::schemeFromString("daylight"), Theme::Light);
    QCOMPARE(Theme::schemeFromString("system"), Theme::Classic);
    QCOMPARE(Theme::schemeFromString("classic"), Theme::Classic);
    QCOMPARE(Theme::schemeFromString("nonsense"), Theme::Dark);
}

void Theme_Test::stylesheetTokens()
{
    Theme::instance()->apply(Theme::OLED, QColor("#2D8CFF"), true, false);

    const QString qss = Theme::instance()->styleSheet();
    QVERIFY(qss.isEmpty() == false);
    QVERIFY(qss.contains("%ACCENT%") == false);
    QVERIFY(qss.contains("%BG%") == false);
    QVERIFY(qss.contains("%RADIUS%") == false);
    QVERIFY(qss.contains("rgba(") == true);
    QVERIFY(qss.contains("QPushButton") == true);
    QVERIFY(qss.contains(":/themes/chevron-down-light.png") == true);

    /* A light scheme must use the dark arrow images */
    Theme::instance()->apply(Theme::Light, QColor("#2D8CFF"), true, false);
    const QString light = Theme::instance()->styleSheet();
    QVERIFY(light.contains(":/themes/chevron-down-dark.png") == true);
    QVERIFY(light.contains("%ACCENT%") == false);
}

void Theme_Test::oledPalette()
{
    Theme::instance()->apply(Theme::OLED, QColor("#2D8CFF"), true, false);

    const QPalette palette = QApplication::palette();
    QCOMPARE(palette.color(QPalette::Window), QColor("#000000"));
    QCOMPARE(palette.color(QPalette::Base), QColor("#000000"));
    QCOMPARE(palette.color(QPalette::Inactive, QPalette::Window), QColor("#000000"));
    QCOMPARE(palette.color(QPalette::Text), QColor("#F4F6FA"));
    QCOMPARE(palette.color(QPalette::Inactive, QPalette::Text), QColor("#F4F6FA"));
    QCOMPARE(palette.color(QPalette::Highlight), QColor("#2D8CFF"));
    QCOMPARE(palette.color(QPalette::HighlightedText), QColor("#FFFFFF"));

    /* rounded look */
    QCOMPARE(Theme::instance()->radius(), 8);
}

void Theme_Test::classicRestoresPalette()
{
    /* Go through a coloured scheme first, then make sure Classic brings the
     * stock palette and an empty stylesheet back. */
    Theme::instance()->apply(Theme::Dark, QColor("#FFB020"), true, false);
    Theme::instance()->apply(Theme::Classic, QColor("#FFB020"), false, false);

    const QString classicQss = Theme::instance()->styleSheet();
    QVERIFY(classicQss.isEmpty() == false);
    QVERIFY(classicQss.contains("#topBar") == true);
    QVERIFY(classicQss.contains("QPushButton") == false);
    QCOMPARE(QApplication::palette(), m_originalPalette);
    QCOMPARE(Theme::instance()->radius(), 4);

    /* Back to OLED for the animation test */
    Theme::instance()->apply(Theme::OLED, QColor("#2D8CFF"), true, false);
}

void Theme_Test::contrastingText()
{
    QCOMPARE(Theme::contrastingText(QColor("#000000")), QColor("#FFFFFF"));
    QCOMPARE(Theme::contrastingText(QColor("#2D8CFF")), QColor("#FFFFFF"));
    QCOMPARE(Theme::contrastingText(QColor("#FFB020")), QColor("#0A0A0C"));
    QCOMPARE(Theme::contrastingText(QColor("#FFFFFF")), QColor("#0A0A0C"));
}

void Theme_Test::hoverAndRippleProgress()
{
    UiEffects *effects = UiEffects::instance();

    QWidget top;
    QVBoxLayout layout(&top);
    QPushButton button("Hover me", &top);
    button.setFixedSize(180, 48);
    layout.addWidget(&button);
    top.resize(260, 120);
    top.show();
    QTest::qWait(20);

    QEvent enter(QEvent::Enter);
    QApplication::sendEvent(&button, &enter);
    QTest::qWait(60);
    QVERIFY(effects->hoverProgress(&button) > 0.0);

    QTest::mousePress(&button, Qt::LeftButton, Qt::NoModifier, QPoint(90, 24));
    QTest::qWait(40);
    QVERIFY(effects->rippleProgress(&button) > 0.0);
    QCOMPARE(effects->rippleOrigin(&button), QPointF(90, 24));

    effects->clearEffects();
    QCOMPARE(effects->hoverProgress(&button), 0.0);
    QCOMPARE(effects->rippleProgress(&button), 0.0);
}

void Theme_Test::iconsAvailable()
{
    Theme::instance()->apply(Theme::Dark, QColor("#2D8CFF"), true, false);

    QVERIFY(AppIcons::hasIcon("lightbulb") == true);
    QVERIFY(AppIcons::hasIcon("play") == true);
    QVERIFY(AppIcons::hasIcon("does-not-exist") == false);

    /* Custom semantic and lamp-type icons must be present and render */
    QVERIFY(AppIcons::hasIcon("fixtures") == true);
    QVERIFY(AppIcons::hasIcon("lamp-movinghead") == true);
    QVERIFY(AppIcons::purposeForName("virtual-console") == AppIcons::Console);
    QVERIFY(AppIcons::purposeForName("lamp-dimmer") == AppIcons::LampDimmer);

    const QPixmap pm = AppIcons::pixmap("lightbulb", 20, QColor("#FFFFFF"));
    QVERIFY(pm.isNull() == false);

    const QPixmap multi = AppIcons::pixmap("fixtures", 20,
                                           AppIcons::Fixtures,
                                           AppIcons::Selected);
    QVERIFY(multi.isNull() == false);

    const QIcon icon = AppIcons::icon("fixtures", 20, AppIcons::Fixtures);
    QVERIFY(icon.isNull() == false);

    AppIcons::clearCache();
}

void Theme_Test::legacyIconColors()
{
    Theme::instance()->apply(Theme::Dark, QColor("#2D8CFF"), true, false);

    /* The restyled legacy icons must keep their original multi-colour
     * identity. A monochrome recolour would collapse to a single RGB value. */
    const QStringList names = QStringList() << "fixtures" << "functions"
                                            << "vc-slider" << "lamp-colorchanger";
    foreach (const QString &name, names)
    {
        const QImage image = AppIcons::pixmap(name, 64,
                                              AppIcons::purposeForName(name),
                                              AppIcons::Normal).toImage();
        QVERIFY(image.isNull() == false);

        QSet<QRgb> colors;
        for (int y = 0; y < image.height(); y++)
        {
            for (int x = 0; x < image.width(); x++)
            {
                const QRgb pixel = image.pixel(x, y);
                if (qAlpha(pixel) > 16)
                    colors.insert(qRgb(qRed(pixel), qGreen(pixel), qBlue(pixel)));
            }
        }
        QVERIFY2(colors.size() >= 3,
                 qPrintable(QString("%1 lost its legacy colours (%2 colours)")
                            .arg(name).arg(colors.size())));
    }

    AppIcons::clearCache();
}

void Theme_Test::componentLibrary()
{
    AppCard card;
    card.setTitle("Card title");
    card.setSubtitle("Card subtitle");
    QVERIFY(card.contentLayout() != NULL);

    AppSwitch toggle;
    QCOMPARE(toggle.isCheckable(), true);
    toggle.show();
    QTest::mouseClick(&toggle, Qt::LeftButton);
    QCOMPARE(toggle.isChecked(), true);

    AppSegmentedControl segmented;
    segmented.addSegment("One");
    segmented.addSegment("Two");
    segmented.resize(220, 34);
    segmented.show();
    QTest::qWait(20);
    QCOMPARE(segmented.currentIndex(), 0);

    QSignalSpy spy(&segmented, SIGNAL(currentChanged(int)));
    QTest::mouseClick(&segmented, Qt::LeftButton, Qt::NoModifier, QPoint(170, 17));
    QCOMPARE(segmented.currentIndex(), 1);
    QCOMPARE(spy.count(), 1);

    AppSettingRow row(QStringLiteral("Animated effects"),
                      QStringLiteral("Hover fades and transitions"));
    QVERIFY(row.findChildren<QLabel *>().size() >= 2);

    AppDialog dialog(QStringLiteral("Dialog title"));
    dialog.setSubtitle(QStringLiteral("Dialog subtitle"));
    dialog.setHeaderIcon(QStringLiteral("fixtures"), 24);
    QVERIFY(dialog.contentLayout() != NULL);
    QVERIFY(dialog.footerLayout() != NULL);
    QVERIFY(dialog.closeButton() != NULL);
    QVERIFY(dialog.addFooterButton(QStringLiteral("Apply"), true) != NULL);

    AppEmptyState empty;
    empty.setIcon(QStringLiteral("fixtures"));
    empty.setTitle(QStringLiteral("Nothing selected"));
    empty.setDescription(QStringLiteral("Select an item to continue."));
    empty.setActionText(QStringLiteral("Add"));
    empty.resize(420, 320);
    empty.show();
    QTest::qWait(20);

    QPushButton *action = empty.findChild<QPushButton *>();
    QVERIFY(action != NULL);
    QSignalSpy emptySpy(&empty, &AppEmptyState::actionTriggered);
    QTest::mouseClick(action, Qt::LeftButton);
    QCOMPARE(emptySpy.count(), 1);
}

QTEST_MAIN(Theme_Test)
