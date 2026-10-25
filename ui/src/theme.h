/*
  Q Light Controller Plus
  theme.h

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

#ifndef THEME_H
#define THEME_H

#include <QObject>
#include <QPalette>
#include <QColor>
#include <QFont>
#include <QString>
#include <QList>
#include <QHash>

/** @addtogroup ui UI
 * @{
 */

/**
 * Central place for the QLC+ 4 application appearance.
 *
 * The theme engine is intentionally lightweight: it builds one stylesheet
 * from a tokenized template and a QPalette, so widgets only get polished
 * once when the theme changes. No per-widget stylesheets are generated and
 * no expensive graphics effects are used.
 *
 * The "Classic" scheme restores the stock platform look, which also means
 * that the user supplied qlcplusStyle.qss (if any) keeps working exactly as
 * before.
 */
class Theme final : public QObject
{
    Q_OBJECT

public:
    /** Built-in appearance schemes. */
    enum Scheme
    {
        OLED = 0,   //!< Pure black, high contrast
        Dark,       //!< Midnight: the modern default
        Light,      //!< Daylight: light and clean
        Classic     //!< Stock platform look
    };

    /** Semantic colors exposed to custom painted widgets. */
    enum ColorRole
    {
        Background = 0,
        Surface,
        SurfaceRaised,
        SurfaceHover,
        Border,
        BorderStrong,
        Text,
        TextDim,
        TextDisabled,
        Accent,
        Danger,
        Warning,
        Success
    };

    static Theme *instance();

    /** Apply the scheme/accent/animation flags stored in QSettings. */
    void applySaved();

    /** Apply a scheme and persist the choice (unless persist is false). */
    void apply(Scheme scheme, const QColor &accent, bool animations, bool persist = true);

    Scheme scheme() const;
    QColor accent() const;
    bool animationsEnabled() const;

    /** Semantic color for custom painted widgets. */
    QColor color(ColorRole role) const;

    /** Corner radius, in pixels, used by the built-in stylesheet. */
    int radius() const;

    /** Readable foreground color for the given background. */
    static QColor contrastingText(const QColor &background);

    static QString schemeToString(Scheme scheme);
    static Scheme schemeFromString(const QString &name);
    static QString schemeDisplayName(Scheme scheme);

    /** Accent colors offered by the appearance menu. */
    static QList<QColor> accentPresets();

    /** Built-in stylesheet for the current/accent combination. */
    QString styleSheet() const;

signals:
    /** Emitted after the application palette/stylesheet have changed. */
    void changed();

private:
    Theme();

    void applyInternal();
    void loadFonts();
    QPalette buildPalette() const;
    void tokenColors(QHash<QString, QString> &tokens) const;

    Scheme m_scheme;
    QColor m_accent;
    bool m_animations;
    QPalette m_originalPalette;
    QPalette m_currentPalette;
    QFont m_originalFont;
    QString m_uiFontFamily;

    Q_DISABLE_COPY(Theme)
};

/** @} */

#endif // THEME_H
