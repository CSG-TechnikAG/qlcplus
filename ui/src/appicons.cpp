/*
  Q Light Controller Plus
  appicons.cpp

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

#include <QSvgRenderer>
#include <QIconEngine>
#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QHash>
#include <QFile>
#include <QStyle>
#include <QDebug>

#include "appicons.h"
#include "theme.h"

namespace
{
    QHash<QString, QIcon> s_iconCache;
    QHash<QString, QPixmap> s_pixmapCache;

    QString colorKey(const QColor &color)
    {
        return color.isValid() ? color.name(QColor::HexArgb) : QStringLiteral("-");
    }

    QString paletteKey(const AppIcons::Palette &palette)
    {
        return QString("%1|%2|%3|%4")
               .arg(colorKey(palette.primary), colorKey(palette.secondary),
                    colorKey(palette.accent), colorKey(palette.fill));
    }

    /**
     * Dynamic icon engine: every paint reads the current theme palette.
     * This means icons created at page construction (toolbars, trees, tabs)
     * automatically change color when the theme or accent changes.
     */
    class ThemeIconEngine final : public QIconEngine
    {
    public:
        ThemeIconEngine(const QString &name, int size, AppIcons::Purpose purpose)
            : m_name(name)
            , m_size(size)
            , m_purpose(purpose)
        {
        }

        QIconEngine *clone() const override
        {
            return new ThemeIconEngine(*this);
        }

        QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
        {
            AppIcons::State iconState = AppIcons::Normal;
            if (mode == QIcon::Disabled)
                iconState = AppIcons::Disabled;
            else if (mode == QIcon::Selected || state == QIcon::On)
                iconState = AppIcons::Selected;
            else if (mode == QIcon::Active)
                iconState = AppIcons::Hover;

            const int pixelSize = size.width() > 0 ? size.width() : m_size;
            const qreal dpr = qApp != NULL ? qApp->devicePixelRatio() : 1.0;
            return AppIcons::pixmap(m_name, pixelSize, m_purpose, iconState, dpr);
        }

        void paint(QPainter *painter, const QRect &rect,
                   QIcon::Mode mode, QIcon::State state) override
        {
            const QPixmap pm = pixmap(rect.size(), mode, state);
            if (pm.isNull() == false)
                painter->drawPixmap(rect, pm);
        }

        QString key() const override
        {
            Theme *theme = Theme::instance();
            return QString("qlc:%1:%2:%3:%4:%5")
                   .arg(m_name).arg(m_size).arg(int(m_purpose))
                   .arg(int(theme->scheme())).arg(theme->accent().name());
        }

    private:
        QString m_name;
        int m_size;
        AppIcons::Purpose m_purpose;
    };
}

namespace AppIcons
{

Palette::Palette()
{
}

Palette::Palette(const QColor &primaryColor, const QColor &secondaryColor,
                 const QColor &accentColor, const QColor &fillColor)
    : primary(primaryColor)
    , secondary(secondaryColor)
    , accent(accentColor)
    , fill(fillColor)
{
}

static QColor themeColor(QColor color, bool lighten)
{
    Theme *theme = Theme::instance();
    if (theme->scheme() == Theme::Light)
        return color.darker(lighten ? 120 : 100);

    return color.lighter(lighten ? 108 : 100);
}

static QColor semanticColor(Purpose purpose)
{
    Theme *theme = Theme::instance();

    switch (purpose)
    {
    case Fixtures:           return themeColor(QColor("#F5B84A"), false);
    case Functions:          return themeColor(QColor("#A78BFA"), false);
    case Shows:              return themeColor(QColor("#FB923C"), false);
    case Console:            return theme->accent();
    case Desk:               return themeColor(QColor("#22D3EE"), false);
    case Geometry:           return themeColor(QColor("#4ADE80"), false);
    case Preview3D:          return themeColor(QColor("#C084FC"), false);
    case IO:                 return themeColor(QColor("#34D399"), false);
    case AddressTool:        return themeColor(QColor("#FF5C6C"), false);
    case DumpDmx:            return themeColor(QColor("#FFB020"), false);
    case Monitor:            return themeColor(QColor("#22D3EE"), false);
    case Blackout:           return theme->color(Theme::Danger);
    case Appearance:         return themeColor(QColor("#E45BFF"), false);
    case Help:               return theme->accent();
    case FileCreate:         return themeColor(QColor("#3DD68C"), false);
    case FileOpen:           return themeColor(QColor("#F5B84A"), false);
    case FileSave:           return themeColor(QColor("#2D8CFF"), false);
    case LiveEdit:           return themeColor(QColor("#A78BFA"), false);
    case Panic:              return theme->color(Theme::Danger);
    case Destructive:        return theme->color(Theme::Danger);
    case Scene:              return themeColor(QColor("#F5B84A"), false);
    case Chaser:             return themeColor(QColor("#4ADE80"), false);
    case Sequence:           return themeColor(QColor("#34D399"), false);
    case EFX:                return themeColor(QColor("#A78BFA"), false);
    case Collection:         return themeColor(QColor("#2D8CFF"), false);
    case RGBMatrix:          return themeColor(QColor("#E45BFF"), false);
    case Script:             return themeColor(QColor("#A3E635"), false);
    case Audio:              return themeColor(QColor("#22D3EE"), false);
    case Video:              return theme->color(Theme::Danger);
    case Path:               return themeColor(QColor("#FB923C"), false);
    case Folder:             return themeColor(QColor("#F5B84A"), false);
    case LampColorChanger:   return themeColor(QColor("#E45BFF"), false);
    case LampDimmer:         return themeColor(QColor("#F5B84A"), false);
    case LampEffect:         return themeColor(QColor("#E45BFF"), false);
    case LampFan:            return themeColor(QColor("#22D3EE"), false);
    case LampFlower:         return themeColor(QColor("#F472B6"), false);
    case LampHazer:          return themeColor(QColor("#94A3B8"), false);
    case LampLaser:          return theme->color(Theme::Danger);
    case LampLEDBarBeams:    return themeColor(QColor("#22D3EE"), false);
    case LampLEDBarPixels:   return themeColor(QColor("#34D399"), false);
    case LampMovingHead:     return themeColor(QColor("#2D8CFF"), false);
    case LampOther:          return theme->color(Theme::TextDim);
    case LampScanner:        return themeColor(QColor("#A78BFA"), false);
    case LampSmoke:          return themeColor(QColor("#94A3B8"), false);
    case LampStrobe:         return themeColor(QColor("#FFB020"), false);
    case Action:
    case Navigation:
    default:
        return theme->accent();
    }
}

Palette palette(Purpose purpose, State state)
{
    Theme *theme = Theme::instance();
    const QColor text = theme->color(Theme::Text);
    const QColor dim = theme->color(Theme::TextDim);
    const QColor disabled = theme->color(Theme::TextDisabled);
    const QColor accent = semanticColor(purpose);

    Palette p;

    switch (state)
    {
    case Hover:
        p.primary = (purpose == Destructive) ? accent.lighter(112) : text;
        p.secondary = p.primary;
        p.accent = accent.lighter(115);
        p.fill = p.accent;
        break;
    case Selected:
        /* Legacy-based icons keep their original literal colors; only the
         * neutral outline follows the state so the identity is preserved. */
        if (purpose == Destructive)
            p.primary = accent;
        else
            p.primary = (purpose == Action || purpose == Navigation) ? accent : text;
        p.secondary = p.primary;
        p.accent = accent;
        p.fill = accent;
        break;
    case Disabled:
        p.primary = disabled;
        p.secondary = disabled;
        p.accent = disabled;
        p.fill = disabled;
        break;
    case Normal:
    default:
        if (purpose == Destructive)
        {
            p.primary = accent;
            p.secondary = accent;
            p.accent = accent;
            p.fill = accent;
            break;
        }
        p.primary = dim;
        p.secondary = dim;
        /* The blackout icon should only become red when it is active or
         * hovered, otherwise it looks permanently engaged. */
        p.accent = (purpose == Blackout) ? dim : accent;
        p.fill = p.accent;
        break;
    }

    return p;
}

Purpose purposeForName(const QString &name)
{
    if (name == "trash-2" || name == "delete" || name == "remove")
        return Destructive;
    if (name.startsWith("vc-"))      return Console;
    if (name == "fixtures")          return Fixtures;
    if (name == "functions")         return Functions;
    if (name == "shows")             return Shows;
    if (name == "virtual-console")   return Console;
    if (name == "simple-desk")       return Desk;
    if (name == "stage-geometry")    return Geometry;
    if (name == "preview-3d")        return Preview3D;
    if (name == "inputs-outputs")    return IO;
    if (name == "address-tool")      return AddressTool;
    if (name == "dump-dmx")          return DumpDmx;
    if (name == "monitor")           return Monitor;
    if (name == "blackout")          return Blackout;
    if (name == "appearance")        return Appearance;
    if (name == "scene")             return Scene;
    if (name == "chaser")            return Chaser;
    if (name == "sequence")          return Sequence;
    if (name == "efx")               return EFX;
    if (name == "collection")        return Collection;
    if (name == "rgbmatrix")         return RGBMatrix;
    if (name == "script")            return Script;
    if (name == "audio")             return Audio;
    if (name == "video")             return Video;
    if (name == "path")              return Path;
    if (name == "folder")            return Folder;
    if (name == "lamp-colorchanger") return LampColorChanger;
    if (name == "lamp-dimmer")       return LampDimmer;
    if (name == "lamp-effect")       return LampEffect;
    if (name == "lamp-fan")          return LampFan;
    if (name == "lamp-flower")       return LampFlower;
    if (name == "lamp-hazer")        return LampHazer;
    if (name == "lamp-laser")        return LampLaser;
    if (name == "lamp-ledbar-beams") return LampLEDBarBeams;
    if (name == "lamp-ledbar-pixels") return LampLEDBarPixels;
    if (name == "lamp-movinghead")   return LampMovingHead;
    if (name == "lamp-other")        return LampOther;
    if (name == "lamp-scanner")      return LampScanner;
    if (name == "lamp-smoke")        return LampSmoke;
    if (name == "lamp-strobe")       return LampStrobe;
    if (name == "file-plus")         return FileCreate;
    if (name == "folder-open")       return FileOpen;
    if (name == "save" || name == "save-all") return FileSave;
    if (name == "pencil-line")       return LiveEdit;
    if (name == "sliders-horizontal") return Console;
    if (name == "scan-line")         return AddressTool;
    if (name == "arrow-down-to-line") return DumpDmx;
    if (name == "power")             return Blackout;
    if (name == "palette")           return Appearance;
    if (name == "circle-help" || name == "info") return Help;
    if (name == "octagon-x")         return Panic;

    return Action;
}

bool hasIcon(const QString &name)
{
    return QFile::exists(QString(":/icons/custom/%1.svg").arg(name))
        || QFile::exists(QString(":/icons/lucide/%1.svg").arg(name));
}

static QPixmap renderCustom(const QString &path, int size, const Palette &p, qreal dpr)
{
    QFile file(path);
    if (file.open(QIODevice::ReadOnly) == false)
        return QPixmap();

    QString svg = QString::fromUtf8(file.readAll());
    file.close();

    const QColor primary = p.primary.isValid() ? p.primary : QColor("#FFFFFF");
    const QColor secondary = p.secondary.isValid() ? p.secondary : primary;
    const QColor accent = p.accent.isValid() ? p.accent : primary;
    const QColor fill = p.fill.isValid() ? p.fill : accent;

    svg.replace("%PRIMARY%", primary.name());
    svg.replace("%SECONDARY%", secondary.name());
    svg.replace("%ACCENT%", accent.name());
    svg.replace("%FILL%", fill.name());

    QSvgRenderer renderer(svg.toUtf8());
    if (renderer.isValid() == false)
        return QPixmap();

    const int pixelSize = qMax(1, qRound(size * (dpr > 0.0 ? dpr : 1.0)));
    QImage image(pixelSize, pixelSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    renderer.render(&painter);
    painter.end();

    QPixmap pm = QPixmap::fromImage(image);
    pm.setDevicePixelRatio(dpr > 0.0 ? dpr : 1.0);
    return pm;
}

static QPixmap renderMask(const QString &path, int size, const QColor &color, qreal dpr)
{
    if (QFile::exists(path) == false)
        return QPixmap();

    const int pixelSize = qMax(1, qRound(size * (dpr > 0.0 ? dpr : 1.0)));
    QImage image(pixelSize, pixelSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QSvgRenderer renderer(path);
    if (renderer.isValid() == false)
        return QPixmap();

    QPainter painter(&image);
    renderer.render(&painter);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(image.rect(), color);
    painter.end();

    QPixmap pm = QPixmap::fromImage(image);
    pm.setDevicePixelRatio(dpr > 0.0 ? dpr : 1.0);
    return pm;
}

QPixmap pixmap(const QString &name, int size, const Palette &palette, qreal dpr)
{
    if (size <= 0)
        return QPixmap();

    const qreal ratio = dpr > 0.0 ? dpr : 1.0;
    const QString key = QString("%1:%2:%3:%4")
                        .arg(name).arg(size).arg(paletteKey(palette)).arg(ratio);
    if (s_pixmapCache.contains(key) == true)
        return s_pixmapCache.value(key);

    const QString customPath = QString(":/icons/custom/%1.svg").arg(name);
    const QString lucidePath = QString(":/icons/lucide/%1.svg").arg(name);

    QPixmap pm;
    if (QFile::exists(customPath) == true)
        pm = renderCustom(customPath, size, palette, ratio);
    else
        pm = renderMask(lucidePath, size, palette.primary, ratio);

    if (pm.isNull() == false)
        s_pixmapCache.insert(key, pm);

    return pm;
}

QPixmap pixmap(const QString &name, int size, const QColor &color, qreal dpr)
{
    if (size <= 0)
        return QPixmap();

    const qreal ratio = dpr > 0.0 ? dpr : 1.0;
    const QString key = QString("%1:%2:%3:%4")
                        .arg(name).arg(size).arg(colorKey(color)).arg(ratio);
    if (s_pixmapCache.contains(key) == true)
        return s_pixmapCache.value(key);

    const QString customPath = QString(":/icons/custom/%1.svg").arg(name);
    const QString lucidePath = QString(":/icons/lucide/%1.svg").arg(name);

    QPixmap pm;
    if (QFile::exists(customPath) == true)
    {
        Palette p(color, color, color, color);
        pm = renderCustom(customPath, size, p, ratio);
    }
    else
    {
        pm = renderMask(lucidePath, size, color, ratio);
    }

    if (pm.isNull() == false)
        s_pixmapCache.insert(key, pm);

    return pm;
}

QPixmap pixmap(const QString &name, int size, Purpose purpose, State state, qreal dpr)
{
    return pixmap(name, size, palette(purpose, state), dpr);
}

QIcon icon(const QString &name, int size, const Palette &p)
{
    const QString key = QString("%1:%2:%3").arg(name).arg(size).arg(paletteKey(p));
    if (s_iconCache.contains(key) == true)
        return s_iconCache.value(key);

    QIcon result;
    const qreal dpr = qApp != NULL ? qApp->devicePixelRatio() : 1.0;
    const QPixmap pm = pixmap(name, size, p, dpr);
    if (pm.isNull() == false)
    {
        result.addPixmap(pm, QIcon::Normal, QIcon::Off);
        result.addPixmap(pm, QIcon::Active, QIcon::Off);
        result.addPixmap(pm, QIcon::Selected, QIcon::Off);
        result.addPixmap(pm, QIcon::Normal, QIcon::On);
        result.addPixmap(pm, QIcon::Active, QIcon::On);
    }

    s_iconCache.insert(key, result);
    return result;
}

QIcon icon(const QString &name, int size, Purpose purpose)
{
    const QString key = QString("%1:%2:%3").arg(name).arg(size).arg(int(purpose));
    if (s_iconCache.contains(key) == true)
        return s_iconCache.value(key);

    QIcon result(new ThemeIconEngine(name, size, purpose));

    /* If the icon does not exist at all, fall back to the legacy PNG */
    if (hasIcon(name) == false)
    {
        const QString legacy = QString(":/%1.png").arg(name);
        if (QFile::exists(legacy) == true)
            result = QIcon(legacy);
        else if (qApp != NULL)
            result = QApplication::style()->standardIcon(QStyle::SP_FileIcon);
    }

    s_iconCache.insert(key, result);
    return result;
}

QIcon icon(const QString &name, int size)
{
    return icon(name, size, purposeForName(name));
}

void clearCache()
{
    s_iconCache.clear();
    s_pixmapCache.clear();
}

} // namespace AppIcons
