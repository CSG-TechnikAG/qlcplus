/*
  Q Light Controller Plus
  theme.cpp

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

#include <QApplication>
#include <QFontDatabase>
#include <QSettings>
#include <QPalette>
#include <QColor>
#include <QDebug>
#include <QFile>
#include <QFont>
#include <QList>

#include "theme.h"
#include "uistyleproxy.h"

#define SETTINGS_UISTYLE        QStringLiteral("workspace/uistyle")
#define SETTINGS_UIACCENT       QStringLiteral("workspace/uiaccent")
#define SETTINGS_UIANIMATIONS   QStringLiteral("workspace/uianimations")

#define DEFAULT_UI_STYLE        QStringLiteral("midnight")
#define DEFAULT_UI_ACCENT       QStringLiteral("#2D8CFF")

#define THEME_RADIUS            8
#define THEME_RADIUS_CLASSIC    4

static Theme *s_theme = NULL;
static bool s_styleProxyInstalled = false;

/**
 * Install the button-panel proxy style once, before any stylesheet is set.
 * QApplication wraps whatever style is current in a QStyleSheetStyle as soon
 * as setStyleSheet() is called, so the proxy must be installed first.
 */
static void ensureStyleProxy()
{
    if (QApplication::instance() == NULL)
        return;

    if (s_styleProxyInstalled == false)
    {
        QStyle *current = QApplication::style();
        if (dynamic_cast<UiStyleProxy *>(current) == NULL)
            QApplication::setStyle(new UiStyleProxy(current));
        s_styleProxyInstalled = true;
    }
}

/** Colors used to build both the palette and the stylesheet tokens. */
struct SchemeColors
{
    QColor background;
    QColor surface;
    QColor surfaceRaised;
    QColor surfaceHover;
    QColor border;
    QColor borderStrong;
    QColor text;
    QColor textDim;
    QColor textDisabled;
    QColor danger;
    QColor warning;
    QColor success;
};

static SchemeColors colorsFor(Theme::Scheme scheme)
{
    SchemeColors c;

    switch (scheme)
    {
    case Theme::OLED:
        /* Every main surface is pure black to save power on OLED panels */
        c.background     = QColor("#000000");
        c.surface        = QColor("#000000");
        c.surfaceRaised  = QColor("#060708");
        c.surfaceHover   = QColor("#0D1014");
        c.border         = QColor("#1A1F27");
        c.borderStrong   = QColor("#2A3140");
        c.text           = QColor("#F4F6FA");
        c.textDim        = QColor("#9AA3B2");
        c.textDisabled   = QColor("#565F6E");
        break;
    case Theme::Dark:
        c.background     = QColor("#0B0D12");
        c.surface        = QColor("#11141B");
        c.surfaceRaised  = QColor("#171B24");
        c.surfaceHover   = QColor("#1E2430");
        c.border         = QColor("#252B36");
        c.borderStrong   = QColor("#333C4D");
        c.text           = QColor("#F2F5FA");
        c.textDim        = QColor("#9AA4B2");
        c.textDisabled   = QColor("#5A6474");
        break;
    case Theme::Light:
        c.background     = QColor("#F5F7FA");
        c.surface        = QColor("#FFFFFF");
        c.surfaceRaised  = QColor("#EEF1F6");
        c.surfaceHover   = QColor("#E6EAF1");
        c.border         = QColor("#DDE2EA");
        c.borderStrong   = QColor("#C8CFDA");
        c.text           = QColor("#141821");
        c.textDim        = QColor("#5E6878");
        c.textDisabled   = QColor("#9AA4B2");
        break;
    case Theme::Classic:
    default:
        c.background     = QColor("#EFEFEF");
        c.surface        = QColor("#FFFFFF");
        c.surfaceRaised  = QColor("#F5F5F5");
        c.surfaceHover   = QColor("#E6E6E6");
        c.border         = QColor("#C8C8C8");
        c.borderStrong   = QColor("#A8A8A8");
        c.text           = QColor("#1A1A1A");
        c.textDim        = QColor("#606060");
        c.textDisabled   = QColor("#9A9A9A");
        break;
    }

    c.danger  = QColor("#FF5C6C");
    c.warning = QColor("#FFB020");
    c.success = QColor("#3DD68C");

    return c;
}

/** Set a role for both the active and inactive groups. */
static void setPaletteRole(QPalette &palette, QPalette::ColorRole role, const QColor &color)
{
    palette.setColor(QPalette::Active, role, color);
    palette.setColor(QPalette::Inactive, role, color);
}

Theme *Theme::instance()
{
    if (s_theme == NULL)
        s_theme = new Theme();
    return s_theme;
}

Theme::Theme()
    : QObject(NULL)
    , m_scheme(OLED)
    , m_accent(DEFAULT_UI_ACCENT)
    , m_animations(true)
{
    if (qApp != NULL)
    {
        m_originalPalette = qApp->palette();
        m_currentPalette = m_originalPalette;
        m_originalFont = qApp->font();
    }
}

void Theme::applySaved()
{
    QSettings settings;

    const QString schemeName = settings.value(SETTINGS_UISTYLE, DEFAULT_UI_STYLE).toString();
    const QColor accent(settings.value(SETTINGS_UIACCENT, DEFAULT_UI_ACCENT).toString());

    m_scheme = schemeFromString(schemeName);
    m_accent = accent.isValid() ? accent : QColor(DEFAULT_UI_ACCENT);
    m_animations = settings.value(SETTINGS_UIANIMATIONS, true).toBool();

    applyInternal();
}

void Theme::apply(Scheme scheme, const QColor &accent, bool animations, bool persist)
{
    m_scheme = scheme;
    m_accent = accent.isValid() ? accent : QColor(DEFAULT_UI_ACCENT);
    m_animations = animations;

    if (persist == true)
    {
        QSettings settings;
        settings.setValue(SETTINGS_UISTYLE, schemeToString(m_scheme));
        settings.setValue(SETTINGS_UIACCENT, m_accent.name());
        settings.setValue(SETTINGS_UIANIMATIONS, m_animations);
    }

    applyInternal();
}

void Theme::loadFonts()
{
    static bool fontsLoaded = false;
    if (fontsLoaded == true || qApp == NULL)
        return;

    fontsLoaded = true;

    const int regular = QFontDatabase::addApplicationFont(":/fonts/Inter-Regular.ttf");
    QFontDatabase::addApplicationFont(":/fonts/Inter-Medium.ttf");
    QFontDatabase::addApplicationFont(":/fonts/Inter-SemiBold.ttf");

    if (regular < 0)
    {
        qWarning() << "[Theme] Unable to load the Inter font, using the system font";
        return;
    }

    const QStringList families = QFontDatabase::applicationFontFamilies(regular);
    if (families.isEmpty() == false)
        m_uiFontFamily = families.first();
}

void Theme::applyInternal()
{
    if (qApp == NULL)
        return;

    loadFonts();

    /* Modern themes use Inter; Classic returns to the original system font. */
    if (m_scheme == Classic)
    {
        if (QApplication::font() != m_originalFont)
            QApplication::setFont(m_originalFont);
    }
    else if (m_uiFontFamily.isEmpty() == false
             && QApplication::font().family() != m_uiFontFamily)
    {
        QFont font(m_uiFontFamily);
        font.setPixelSize(14);
        font.setStyleStrategy(QFont::PreferAntialias);
        QApplication::setFont(font);
    }

    /* Must happen before setStyleSheet(), see ensureStyleProxy() */
    ensureStyleProxy();

    /* Build the palette first, then the stylesheet. Some custom painted
     * widgets (virtual console, EFX preview, monitor) only look at the
     * palette, so a dark scheme must be reflected there as well. */
    m_currentPalette = buildPalette();
    qApp->setPalette(m_currentPalette);
    qApp->setStyleSheet(styleSheet());

    emit changed();
}

QPalette Theme::buildPalette() const
{
    if (m_scheme == Classic)
        return m_originalPalette;

    const SchemeColors c = colorsFor(m_scheme);
    const QColor onAccent = contrastingText(m_accent);

    QPalette p;
    setPaletteRole(p, QPalette::Window, c.background);
    setPaletteRole(p, QPalette::WindowText, c.text);
    setPaletteRole(p, QPalette::Base, c.surface);
    setPaletteRole(p, QPalette::AlternateBase, c.surfaceRaised);
    setPaletteRole(p, QPalette::Text, c.text);
    setPaletteRole(p, QPalette::Button, c.surfaceRaised);
    setPaletteRole(p, QPalette::ButtonText, c.text);
    setPaletteRole(p, QPalette::BrightText, QColor("#FFFFFF"));
    setPaletteRole(p, QPalette::ToolTipBase, c.surfaceHover);
    setPaletteRole(p, QPalette::ToolTipText, c.text);
    setPaletteRole(p, QPalette::Highlight, m_accent);
    setPaletteRole(p, QPalette::HighlightedText, onAccent);
    setPaletteRole(p, QPalette::Link, m_accent);
    setPaletteRole(p, QPalette::LinkVisited, m_accent.darker(120));

    setPaletteRole(p, QPalette::Light, c.background.lighter(140));
    setPaletteRole(p, QPalette::Midlight, c.background.lighter(115));
    setPaletteRole(p, QPalette::Mid, c.borderStrong);
    setPaletteRole(p, QPalette::Dark, c.border);
    setPaletteRole(p, QPalette::Shadow, c.background.darker(160));

#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
    setPaletteRole(p, QPalette::PlaceholderText, c.textDim);
#endif

    p.setColor(QPalette::Disabled, QPalette::WindowText, c.textDisabled);
    p.setColor(QPalette::Disabled, QPalette::Window, c.background);
    p.setColor(QPalette::Disabled, QPalette::Text, c.textDisabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, c.textDisabled);
    p.setColor(QPalette::Disabled, QPalette::Base, c.surface);
    p.setColor(QPalette::Disabled, QPalette::AlternateBase, c.surfaceRaised);
    p.setColor(QPalette::Disabled, QPalette::Button, c.surfaceRaised);
    p.setColor(QPalette::Disabled, QPalette::BrightText, c.textDisabled);
    p.setColor(QPalette::Disabled, QPalette::ToolTipBase, c.surfaceHover);
    p.setColor(QPalette::Disabled, QPalette::ToolTipText, c.textDisabled);
    p.setColor(QPalette::Disabled, QPalette::Link, c.textDisabled);
    p.setColor(QPalette::Disabled, QPalette::Highlight, m_accent.darker(150));
    p.setColor(QPalette::Disabled, QPalette::HighlightedText, c.textDisabled);

    return p;
}

void Theme::tokenColors(QHash<QString, QString> &tokens) const
{
    const SchemeColors c = colorsFor(m_scheme);
    const QColor hover = m_accent.lighter(112);
    const QColor press = m_accent.darker(125);

    tokens.insert("%BG%", c.background.name());
    tokens.insert("%SURFACE%", c.surface.name());
    tokens.insert("%SURFACE2%", c.surfaceRaised.name());
    tokens.insert("%SURFACE3%", c.surfaceHover.name());
    tokens.insert("%BORDER%", c.border.name());
    tokens.insert("%BORDER_STRONG%", c.borderStrong.name());
    tokens.insert("%TEXT%", c.text.name());
    tokens.insert("%TEXT_DIM%", c.textDim.name());
    tokens.insert("%TEXT_DISABLED%", c.textDisabled.name());
    tokens.insert("%ACCENT%", m_accent.name());
    tokens.insert("%ACCENT_HOVER%", hover.name());
    tokens.insert("%ACCENT_PRESS%", press.name());
    tokens.insert("%ACCENT_TEXT%", contrastingText(m_accent).name());
    tokens.insert("%ACCENT_PRESS_TEXT%", contrastingText(press).name());
    tokens.insert("%ACCENT_SOFT%", QString("rgba(%1, %2, %3, 0.16)")
                  .arg(m_accent.red()).arg(m_accent.green()).arg(m_accent.blue()));
    tokens.insert("%ACCENT_FAINT%", QString("rgba(%1, %2, %3, 0.08)")
                  .arg(m_accent.red()).arg(m_accent.green()).arg(m_accent.blue()));
    tokens.insert("%DANGER%", c.danger.name());
    tokens.insert("%DANGER_TEXT%", contrastingText(c.danger).name());
    tokens.insert("%DANGER_PRESS_TEXT%", contrastingText(c.danger.darker(115)).name());
    tokens.insert("%SUCCESS_TEXT%", contrastingText(c.success).name());
    tokens.insert("%WARNING%", c.warning.name());
    tokens.insert("%SUCCESS%", c.success.name());
    tokens.insert("%DANGER_SOFT%", QString("rgba(%1, %2, %3, 0.16)")
                  .arg(c.danger.red()).arg(c.danger.green()).arg(c.danger.blue()));
    tokens.insert("%RADIUS%", QString::number(THEME_RADIUS));
    tokens.insert("%RADIUS_SM%", QString::number(THEME_RADIUS - 2));
    tokens.insert("%RADIUS_LG%", QString::number(THEME_RADIUS + 4));

    const bool darkSurface = (m_scheme == OLED || m_scheme == Dark);
    tokens.insert("%ARROW_UP%", darkSurface ? ":/themes/chevron-up-light.png"
                                            : ":/themes/chevron-up-dark.png");
    tokens.insert("%ARROW_DOWN%", darkSurface ? ":/themes/chevron-down-light.png"
                                              : ":/themes/chevron-down-dark.png");
    tokens.insert("%ARROW_RIGHT%", darkSurface ? ":/themes/chevron-right-light.png"
                                               : ":/themes/chevron-right-dark.png");
    /* Check mark drawn on an accent-filled indicator. */
    tokens.insert("%CHECK%", contrastingText(m_accent) == QColor("#0A0A0C")
                             ? ":/themes/check-dark.png" : ":/themes/check-light.png");
}

QString Theme::styleSheet() const
{
    QFile shellFile(":/themes/shell.qss");
    if (shellFile.open(QIODevice::ReadOnly) == false)
    {
        qWarning() << "[Theme] Unable to load the shell stylesheet";
        return QString();
    }

    const QString shell = QString::fromUtf8(shellFile.readAll());
    shellFile.close();

    QString qss;
    if (m_scheme != Classic)
    {
        QFile widgetsFile(":/themes/base.qss");
        if (widgetsFile.open(QIODevice::ReadOnly) == true)
        {
            qss = QString::fromUtf8(widgetsFile.readAll());
            widgetsFile.close();
        }
        else
        {
            qWarning() << "[Theme] Unable to load the widget stylesheet";
        }
    }

    /* Shell rules win over generic widget rules with the same specificity */
    qss += shell;

    QHash<QString, QString> tokens;
    tokenColors(tokens);

    QHash<QString, QString>::const_iterator it = tokens.constBegin();
    while (it != tokens.constEnd())
    {
        qss.replace(it.key(), it.value());
        ++it;
    }

    return qss;
}

Theme::Scheme Theme::scheme() const
{
    return m_scheme;
}

QColor Theme::accent() const
{
    return m_accent;
}

bool Theme::animationsEnabled() const
{
    return m_animations;
}

QColor Theme::color(ColorRole role) const
{
    const SchemeColors c = colorsFor(m_scheme);

    switch (role)
    {
    case Background:    return c.background;
    case Surface:       return c.surface;
    case SurfaceRaised: return c.surfaceRaised;
    case SurfaceHover:  return c.surfaceHover;
    case Border:        return c.border;
    case BorderStrong:  return c.borderStrong;
    case Text:          return c.text;
    case TextDim:       return c.textDim;
    case TextDisabled:  return c.textDisabled;
    case Accent:        return m_accent;
    case Danger:        return c.danger;
    case Warning:       return c.warning;
    case Success:       return c.success;
    default:            return c.text;
    }
}

int Theme::radius() const
{
    return m_scheme == Classic ? THEME_RADIUS_CLASSIC : THEME_RADIUS;
}

QColor Theme::contrastingText(const QColor &background)
{
    /* Relative luminance, good enough for picking black or white */
    const double luminance = 0.2126 * background.redF()
                           + 0.7152 * background.greenF()
                           + 0.0722 * background.blueF();

    return luminance > 0.55 ? QColor("#0A0A0C") : QColor("#FFFFFF");
}

QString Theme::schemeToString(Scheme scheme)
{
    switch (scheme)
    {
    case OLED:    return QStringLiteral("oled");
    case Dark:    return QStringLiteral("midnight");
    case Light:   return QStringLiteral("light");
    case Classic: return QStringLiteral("classic");
    default:      return DEFAULT_UI_STYLE;
    }
}

Theme::Scheme Theme::schemeFromString(const QString &name)
{
    const QString n = name.trimmed().toLower();

    if (n == "classic" || n == "system")
        return Classic;
    if (n == "light" || n == "daylight")
        return Light;
    if (n == "oled")
        return OLED;
    if (n == "dark" || n == "midnight")
        return Dark;

    return Dark;
}

QString Theme::schemeDisplayName(Scheme scheme)
{
    switch (scheme)
    {
    case OLED:    return tr("OLED");
    case Dark:    return tr("Midnight");
    case Light:   return tr("Daylight");
    case Classic: return tr("Classic");
    default:      return tr("Midnight");
    }
}

QList<QColor> Theme::accentPresets()
{
    QList<QColor> list;
    list << QColor("#2D8CFF")   // QLC+ blue
         << QColor("#00C2D1")   // cyan
         << QColor("#3DDC84")   // green
         << QColor("#FFB020")   // amber
         << QColor("#FF5A5F")   // red
         << QColor("#E45BFF")   // magenta
         << QColor("#8A6BFF");  // purple

    return list;
}
