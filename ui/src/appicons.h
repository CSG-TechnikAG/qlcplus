/*
  Q Light Controller Plus
  appicons.h

  Copyright (c) Massimo Callegari

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

#ifndef APPICONS_H
#define APPICONS_H

#include <QPixmap>
#include <QIcon>
#include <QColor>
#include <QString>

/** @addtogroup ui UI
 * @{
 */

/**
 * Theme-aware, multi-color icon provider.
 *
 * Icons come from two sets:
 *  - bundled Lucide SVGs for generic UI actions (single color, mask tinted)
 *  - custom QLC+ SVGs in ui/src/icons/custom using %PRIMARY%, %SECONDARY%,
 *    %ACCENT% and %FILL% tokens for purpose-specific multi-color icons
 *
 * Each semantic purpose has normal, hover, selected and disabled palettes.
 * QIcon variants carry those palettes, so tree items, toolbars and the
 * navigation rail automatically change color with state and theme.
 */
namespace AppIcons
{
    struct Palette
    {
        QColor primary;
        QColor secondary;
        QColor accent;
        QColor fill;

        Palette();
        Palette(const QColor &primaryColor,
                const QColor &secondaryColor = QColor(),
                const QColor &accentColor = QColor(),
                const QColor &fillColor = QColor());
    };

    enum Purpose
    {
        Action = 0,
        Navigation,
        Destructive,
        Fixtures,
        Functions,
        Shows,
        Console,
        Desk,
        Geometry,
        Preview3D,
        IO,
        AddressTool,
        DumpDmx,
        Monitor,
        Blackout,
        Appearance,
        Help,
        FileCreate,
        FileOpen,
        FileSave,
        LiveEdit,
        Panic,
        Scene,
        Chaser,
        Sequence,
        EFX,
        Collection,
        RGBMatrix,
        Script,
        Audio,
        Video,
        Path,
        Folder,
        LampColorChanger,
        LampDimmer,
        LampEffect,
        LampFan,
        LampFlower,
        LampHazer,
        LampLaser,
        LampLEDBarBeams,
        LampLEDBarPixels,
        LampMovingHead,
        LampOther,
        LampScanner,
        LampSmoke,
        LampStrobe
    };

    enum State
    {
        Normal = 0,
        Hover,
        Selected,
        Disabled
    };

    /** Icon using the Action purpose. */
    QIcon icon(const QString &name, int size = 20);

    /** Icon with a semantic purpose (normal/hover/selected/disabled). */
    QIcon icon(const QString &name, int size, Purpose purpose);

    /** Icon with one explicit palette. */
    QIcon icon(const QString &name, int size, const Palette &palette);

    /** Single-color pixmap (legacy and special cases). */
    QPixmap pixmap(const QString &name, int size, const QColor &color, qreal dpr = 1.0);

    /** Multi-color pixmap with an explicit palette. */
    QPixmap pixmap(const QString &name, int size, const Palette &palette, qreal dpr = 1.0);

    /** Multi-color pixmap for a purpose and state. */
    QPixmap pixmap(const QString &name, int size, Purpose purpose, State state,
                   qreal dpr = 1.0);

    Palette palette(Purpose purpose, State state);
    Purpose purposeForName(const QString &name);

    bool hasIcon(const QString &name);

    /** Forget cached icons after a theme change. */
    void clearCache();
}

/** @} */

#endif // APPICONS_H
