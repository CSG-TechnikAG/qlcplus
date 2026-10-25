/*
  Q Light Controller Plus
  mainshell.cpp

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

#include <QAbstractItemView>
#include <QBoxLayout>
#include <QSettings>
#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QIcon>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QSet>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShortcut>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <QStyle>
#include <QStyleOption>
#include <algorithm>
#include <functional>

#include "mainshell.h"
#include "appsegmentedcontrol.h"
#include "appicons.h"
#include "theme.h"
#include "qlcconfig.h"

/* ------------------------------------------------------------------ */

static QColor mixColors(const QColor &from, const QColor &to, qreal progress)
{
    const qreal t = qBound(0.0, progress, 1.0);
    return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * t,
                            from.greenF() + (to.greenF() - from.greenF()) * t,
                            from.blueF() + (to.blueF() - from.blueF()) * t);
}

static QFont uiFont(int pixelSize, QFont::Weight weight = QFont::Normal)
{
    QFont font = qApp != NULL ? qApp->font() : QFont();
    font.setPixelSize(pixelSize);
    font.setWeight(weight);
    return font;
}

static int motionDuration(int duration)
{
    Theme *theme = Theme::instance();
    return (theme->animationsEnabled() && theme->scheme() != Theme::Classic)
           ? duration : 0;
}

/* ------------------------------------------------------------------ */
/* Navigation rail                                                     */
/* ------------------------------------------------------------------ */

class NavigationRail final : public QWidget
{
public:
    struct Item
    {
        QString iconName;
        QIcon fallbackIcon;
        QString title;
    };

    typedef std::function<void(int)> IndexHandler;
    typedef std::function<void(int, const QPoint &)> ContextHandler;

    explicit NavigationRail(QWidget *parent = NULL)
        : QWidget(parent)
        , m_orientation(Qt::Vertical)
        , m_activeIndex(-1)
        , m_hoverIndex(-1)
        , m_pressedIndex(-1)
        , m_hoverProgress(0.0)
        , m_indicatorPos(-1.0)
    {
        setMouseTracking(true);
        setOrientation(Qt::Vertical);

        m_hoverAnim = new QVariantAnimation(this);
        m_hoverAnim->setStartValue(0.0);
        m_hoverAnim->setEndValue(1.0);
        m_hoverAnim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_hoverAnim, &QVariantAnimation::valueChanged,
                this, [this](const QVariant &value)
        {
            m_hoverProgress = value.toReal();
            update();
        });

        m_indicatorAnim = new QVariantAnimation(this);
        m_indicatorAnim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_indicatorAnim, &QVariantAnimation::valueChanged,
                this, [this](const QVariant &value)
        {
            m_indicatorPos = value.toReal();
            update();
        });
    }

    void setOrientation(Qt::Orientation orientation)
    {
        m_orientation = orientation;
        if (m_indicatorAnim != NULL)
            m_indicatorAnim->stop();

        if (orientation == Qt::Vertical)
        {
            setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
            setMinimumWidth(224);
            setMaximumWidth(224);
            setMinimumHeight(0);
            setMaximumHeight(QWIDGETSIZE_MAX);
        }
        else
        {
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            setMinimumWidth(0);
            setMaximumWidth(QWIDGETSIZE_MAX);
            setMinimumHeight(54);
            setMaximumHeight(54);
        }

        m_indicatorPos = m_activeIndex >= 0 ? itemStart(m_activeIndex) : -1.0;
        updateGeometry();
        update();
    }

    Qt::Orientation orientation() const
    {
        return m_orientation;
    }

    void setItems(const QList<Item> &items)
    {
        m_items = items;
        m_hoverIndex = -1;
        m_pressedIndex = -1;
        if (m_activeIndex >= m_items.size())
            m_activeIndex = m_items.isEmpty() ? -1 : 0;
        m_indicatorPos = m_activeIndex >= 0 ? itemStart(m_activeIndex) : -1.0;
        update();
    }

    void setActiveIndex(int index, bool animate)
    {
        if (index < 0 || index >= m_items.size())
            index = -1;

        const int oldIndex = m_activeIndex;
        m_activeIndex = index;

        if (index < 0)
        {
            m_indicatorAnim->stop();
            m_indicatorPos = -1.0;
            update();
            return;
        }

        const qreal target = itemStart(index);
        if (animate == false || oldIndex == index || m_indicatorPos < 0.0
            || motionDuration(220) == 0)
        {
            m_indicatorAnim->stop();
            m_indicatorPos = target;
            update();
            return;
        }

        m_indicatorAnim->stop();
        m_indicatorAnim->setDuration(motionDuration(220));
        m_indicatorAnim->setStartValue(m_indicatorPos);
        m_indicatorAnim->setEndValue(target);
        m_indicatorAnim->start();
    }

    int activeIndex() const
    {
        return m_activeIndex;
    }

    IndexHandler clicked;
    IndexHandler doubleClicked;
    ContextHandler contextMenuRequested;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        Theme *theme = Theme::instance();
        const QColor bg = theme->color(Theme::Surface);
        const QColor border = theme->color(Theme::Border);
        const QColor text = theme->color(Theme::Text);
        const QColor textDim = theme->color(Theme::TextDim);
        const QColor accent = theme->accent();
        const bool vertical = (m_orientation == Qt::Vertical);

        painter.fillRect(rect(), bg);
        painter.setPen(border);
        if (vertical)
            painter.drawLine(width() - 1, 0, width() - 1, height());
        else
            painter.drawLine(0, 0, width(), 0);

        if (vertical)
        {
            painter.setFont(uiFont(10, QFont::DemiBold));
            painter.setPen(textDim);
            painter.drawText(QRect(20, 16, width() - 30, 16),
                             Qt::AlignLeft | Qt::AlignVCenter, tr("WORKSPACE"));
        }

        if (m_activeIndex >= 0 && m_activeIndex < m_items.size())
        {
            QRectF pill = itemRect(m_activeIndex);
            if (vertical && m_indicatorPos >= 0.0)
                pill.moveTop(m_indicatorPos);
            else if (!vertical && m_indicatorPos >= 0.0)
                pill.moveLeft(m_indicatorPos);

            QColor pillColor = accent;
            pillColor.setAlphaF(0.13);
            painter.setPen(Qt::NoPen);
            painter.setBrush(pillColor);
            painter.drawRoundedRect(pill, 10, 10);

            painter.setBrush(accent);
            if (vertical)
                painter.drawRoundedRect(QRectF(2.5, pill.top() + 11, 3, pill.height() - 22), 1.5, 1.5);
            else
                painter.drawRoundedRect(QRectF(pill.left() + 12, height() - 3, pill.width() - 24, 3), 1.5, 1.5);
        }

        for (int i = 0; i < m_items.size(); i++)
        {
            const QRect item = itemRect(i);
            const bool active = (i == m_activeIndex);
            const bool hovered = (i == m_hoverIndex && m_hoverProgress > 0.001);
            const bool pressed = (i == m_pressedIndex);

            if (hovered == true && active == false)
            {
                QColor hoverColor = mixColors(bg, theme->color(Theme::SurfaceHover),
                                              m_hoverProgress);
                painter.setPen(Qt::NoPen);
                painter.setBrush(hoverColor);
                painter.drawRoundedRect(item, 10, 10);
            }
            else if (pressed == true)
            {
                painter.setPen(Qt::NoPen);
                painter.setBrush(theme->color(Theme::SurfaceHover));
                painter.drawRoundedRect(item, 10, 10);
            }

            const AppIcons::Purpose purpose = AppIcons::purposeForName(m_items[i].iconName);
            const AppIcons::State iconState = active ? AppIcons::Selected
                                             : (hovered ? AppIcons::Hover
                                                        : AppIcons::Normal);
            const int iconSize = vertical ? 22 : 24;
            const QPixmap icon = AppIcons::pixmap(m_items[i].iconName, iconSize,
                                                  purpose, iconState,
                                                  devicePixelRatioF());
            const QPoint iconPos = vertical
                                 ? QPoint(item.left() + 14, item.center().y() - 11)
                                 : QPoint(item.left() + 10, item.center().y() - 12);
            if (icon.isNull() == false)
            {
                painter.drawPixmap(iconPos, icon);
            }
            else if (m_items[i].fallbackIcon.isNull() == false)
            {
                painter.drawPixmap(iconPos, m_items[i].fallbackIcon.pixmap(iconSize, iconSize));
            }

            painter.setPen(active ? text : (hovered ? text : textDim));
            if (vertical)
            {
                painter.setFont(uiFont(14, active ? QFont::DemiBold : QFont::Medium));
                const QRect textRect(item.left() + 50, item.top(),
                                     item.width() - 60, item.height());
                const QString elided = QFontMetrics(painter.font())
                                       .elidedText(m_items[i].title, Qt::ElideRight,
                                                   textRect.width());
                painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, elided);
            }
            else
            {
                painter.setFont(uiFont(13, active ? QFont::DemiBold : QFont::Medium));
                const QRect textRect(item.left() + 38, item.top(),
                                     item.width() - 42, item.height());
                const QString elided = QFontMetrics(painter.font())
                                       .elidedText(m_items[i].title, Qt::ElideRight,
                                                   textRect.width());
                painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, elided);
            }
        }

        if (vertical)
        {
            const int footerY = height() - 38;
            painter.setPen(border);
            painter.drawLine(16, footerY, width() - 16, footerY);
            painter.setFont(uiFont(10));
            painter.setPen(theme->color(Theme::TextDisabled));
            painter.drawText(QRect(16, footerY + 4, width() - 32, 24),
                             Qt::AlignCenter, QString("QLC+ %1").arg(APPVERSION));
        }
    }

    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        m_indicatorAnim->stop();
        if (m_activeIndex >= 0)
            m_indicatorPos = itemStart(m_activeIndex);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        setHoverIndex(itemAt(event->pos()));
        QWidget::mouseMoveEvent(event);
    }

    void leaveEvent(QEvent *event) override
    {
        setHoverIndex(-1);
        QWidget::leaveEvent(event);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            m_pressedIndex = itemAt(event->pos());
            update();
        }
        QWidget::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            const int index = itemAt(event->pos());
            const int pressed = m_pressedIndex;
            m_pressedIndex = -1;
            update();

            if (index >= 0 && index == pressed && clicked)
                clicked(index);
        }
        QWidget::mouseReleaseEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            const int index = itemAt(event->pos());
            if (index >= 0 && doubleClicked)
                doubleClicked(index);
        }
        QWidget::mouseDoubleClickEvent(event);
    }

    void contextMenuEvent(QContextMenuEvent *event) override
    {
        const int index = itemAt(event->pos());
        if (index >= 0 && contextMenuRequested)
            contextMenuRequested(index, event->globalPos());
        event->accept();
    }

private:
    static const int SideMargin = 10;
    static const int TopMargin = 48;
    static const int ItemHeight = 42;
    static const int ItemSpacing = 4;

    static int itemTop(int index)
    {
        return TopMargin + index * (ItemHeight + ItemSpacing);
    }

    QRect itemRect(int index) const
    {
        if (m_orientation == Qt::Vertical)
            return QRect(SideMargin, itemTop(index),
                         width() - SideMargin * 2, ItemHeight);

        const int margin = 8;
        const int gap = 2;
        const int count = qMax(1, m_items.size());
        const int itemWidth = (width() - margin * 2 - gap * (count - 1)) / count;
        return QRect(margin + index * (itemWidth + gap), 6,
                     itemWidth, height() - 12);
    }

    qreal itemStart(int index) const
    {
        const QRect r = itemRect(index);
        return (m_orientation == Qt::Vertical) ? r.top() : r.left();
    }

    int itemAt(const QPoint &pos) const
    {
        for (int i = 0; i < m_items.size(); i++)
        {
            if (itemRect(i).contains(pos) == true)
                return i;
        }
        return -1;
    }

    void setHoverIndex(int index)
    {
        if (index == m_hoverIndex)
            return;

        const bool changingItem = (index >= 0 && m_hoverIndex != index);
        m_hoverIndex = index;

        m_hoverAnim->stop();
        if (motionDuration(120) == 0)
        {
            m_hoverProgress = index >= 0 ? 1.0 : 0.0;
            update();
            return;
        }

        m_hoverAnim->setDuration(motionDuration(120));
        m_hoverAnim->setStartValue(changingItem ? 0.0 : m_hoverProgress);
        m_hoverAnim->setEndValue(index >= 0 ? 1.0 : 0.0);
        m_hoverAnim->start();
    }

private:
    QList<Item> m_items;
    Qt::Orientation m_orientation;
    int m_activeIndex;
    int m_hoverIndex;
    int m_pressedIndex;
    qreal m_hoverProgress;
    qreal m_indicatorPos;
    QVariantAnimation *m_hoverAnim;
    QVariantAnimation *m_indicatorAnim;
};

/* ------------------------------------------------------------------ */
/* Page transition overlay                                             */
/* ------------------------------------------------------------------ */

class PageTransition final : public QWidget
{
public:
    PageTransition(QWidget *parent, const QPixmap &previous)
        : QWidget(parent)
        , m_pixmap(previous)
        , m_progress(0.0)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setGeometry(parent->rect());

        if (size().isEmpty() == false)
        {
            m_pixmap = m_pixmap.scaled(size(), Qt::IgnoreAspectRatio,
                                       Qt::SmoothTransformation);
        }

        m_anim = new QVariantAnimation(this);
        m_anim->setDuration(motionDuration(220));
        m_anim->setStartValue(0.0);
        m_anim->setEndValue(1.0);
        m_anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_anim, &QVariantAnimation::valueChanged,
                this, [this](const QVariant &value)
        {
            m_progress = value.toReal();
            update();
        });
        connect(m_anim, &QVariantAnimation::finished, this, &QObject::deleteLater);

        show();
        raise();
        m_anim->start();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (m_pixmap.isNull())
            return;

        QPainter painter(this);
        painter.setOpacity(1.0 - m_progress);
        painter.fillRect(rect(), Theme::instance()->color(Theme::Background));
        painter.drawPixmap(0, qRound(-6.0 * m_progress), m_pixmap);
    }

private:
    QPixmap m_pixmap;
    qreal m_progress;
    QVariantAnimation *m_anim;
};

/* ------------------------------------------------------------------ */
/* Toast                                                               */
/* ------------------------------------------------------------------ */

class ToastWidget final : public QWidget
{
public:
    explicit ToastWidget(QWidget *parent)
        : QWidget(parent)
        , m_error(false)
        , m_progress(0.0)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        hide();

        m_anim = new QVariantAnimation(this);
        m_anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_anim, &QVariantAnimation::valueChanged,
                this, [this](const QVariant &value)
        {
            m_progress = value.toReal();
            update();
        });
        connect(m_anim, &QVariantAnimation::finished, this, [this]()
        {
            if (m_progress <= 0.001)
                hide();
        });

        m_hideTimer = new QTimer(this);
        m_hideTimer->setSingleShot(true);
        connect(m_hideTimer, &QTimer::timeout, this, [this]()
        {
            m_anim->stop();
            m_anim->setDuration(motionDuration(160));
            m_anim->setStartValue(m_progress);
            m_anim->setEndValue(0.0);
            m_anim->start();
        });
    }

    void showMessage(const QString &text, bool error)
    {
        m_text = text;
        m_error = error;

        QFontMetrics fm(uiFont(13));
        const int maxWidth = qMax(240, parentWidget()->width() - 80);
        const int width = qBound(240, fm.horizontalAdvance(text) + 76, maxWidth);
        const int height = 52;

        resize(width, height);
        move((parentWidget()->width() - width) / 2,
             parentWidget()->height() - height - 24);

        show();
        raise();

        m_hideTimer->stop();
        m_anim->stop();

        if (m_progress < 0.99)
        {
            m_anim->setDuration(motionDuration(200));
            m_anim->setStartValue(0.0);
            m_anim->setEndValue(1.0);
            m_anim->start();
        }
        else
        {
            update();
        }

        m_hideTimer->start(2800);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (m_progress <= 0.001)
            return;

        Theme *theme = Theme::instance();
        const QColor surface = theme->color(Theme::SurfaceRaised);
        const QColor border = theme->color(Theme::BorderStrong);
        const QColor text = theme->color(Theme::Text);

        QPainter painter(this);
        painter.setOpacity(m_progress);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.translate(0, (1.0 - m_progress) * 8.0);

        const QRectF rect = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        painter.setPen(QPen(border, 1));
        painter.setBrush(surface);
        painter.drawRoundedRect(rect, 12, 12);

        const QColor iconColor = m_error ? theme->color(Theme::Danger)
                                         : theme->color(Theme::Success);
        const QPixmap icon = AppIcons::pixmap(m_error ? "circle-alert" : "circle-check",
                                              18, iconColor, devicePixelRatioF());
        if (icon.isNull() == false)
            painter.drawPixmap(18, (height() - 18) / 2, icon);

        painter.setFont(uiFont(13, QFont::Medium));
        painter.setPen(text);
        painter.drawText(QRect(48, 0, width() - 64, height()),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QFontMetrics(painter.font()).elidedText(m_text, Qt::ElideRight,
                                                                 width() - 64));
    }

private:
    QString m_text;
    bool m_error;
    qreal m_progress;
    QVariantAnimation *m_anim;
    QTimer *m_hideTimer;
};

/* ------------------------------------------------------------------ */
/* Command palette                                                     */
/* ------------------------------------------------------------------ */

class CommandPalette final : public QWidget
{
public:
    explicit CommandPalette(QWidget *parent)
        : QWidget(parent)
        , m_card(NULL)
        , m_search(NULL)
        , m_list(NULL)
        , m_hint(NULL)
        , m_cardEffect(NULL)
        , m_anim(NULL)
        , m_backdrop(0.0)
    {
        setFocusPolicy(Qt::StrongFocus);
        hide();

        m_card = new QFrame(this);
        m_card->setObjectName("commandPaletteCard");

        QVBoxLayout *cardLayout = new QVBoxLayout(m_card);
        cardLayout->setContentsMargins(0, 0, 0, 0);
        cardLayout->setSpacing(0);

        m_search = new QLineEdit(m_card);
        m_search->setObjectName("commandSearch");
        m_search->setPlaceholderText(tr("Search commands..."));
        m_search->setFrame(false);
        m_search->setMinimumHeight(46);
        m_search->addAction(AppIcons::icon("search", 18), QLineEdit::LeadingPosition);
        cardLayout->addWidget(m_search);

        QFrame *separator = new QFrame(m_card);
        separator->setObjectName("commandSeparator");
        separator->setFrameShape(QFrame::HLine);
        cardLayout->addWidget(separator);

        m_list = new QListWidget(m_card);
        m_list->setObjectName("commandList");
        m_list->setIconSize(QSize(20, 20));
        m_list->setUniformItemSizes(true);
        m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_list->setSelectionMode(QAbstractItemView::SingleSelection);
        m_list->setFrameShape(QFrame::NoFrame);
        cardLayout->addWidget(m_list, 1);

        m_hint = new QLabel(tr("Up/Down: navigate    Enter: run    Esc: close"), m_card);
        m_hint->setObjectName("commandHint");
        m_hint->setMinimumHeight(30);
        cardLayout->addWidget(m_hint);

        m_cardEffect = new QGraphicsOpacityEffect(m_card);
        m_cardEffect->setOpacity(0.0);
        m_card->setGraphicsEffect(m_cardEffect);

        m_anim = new QVariantAnimation(this);
        m_anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_anim, &QVariantAnimation::valueChanged,
                this, [this](const QVariant &value)
        {
            m_backdrop = value.toReal();
            m_cardEffect->setOpacity(m_backdrop);
            update();
        });
        connect(m_anim, &QVariantAnimation::finished, this, [this]()
        {
            if (m_backdrop <= 0.001)
                hide();
        });

        m_search->installEventFilter(this);
        connect(m_search, &QLineEdit::textChanged,
                this, [this](const QString &text) { filter(text); });
        connect(m_list, &QListWidget::itemActivated,
                this, [this](QListWidgetItem *) { triggerCurrent(); });
    }

    void open()
    {
        if (parentWidget() == NULL)
            return;

        setGeometry(parentWidget()->rect());
        relayout();
        rebuildActions();
        m_search->clear();
        filter(QString());
        show();
        raise();
        m_search->setFocus();

        m_anim->stop();
        m_anim->setDuration(motionDuration(160));
        m_anim->setStartValue(0.0);
        m_anim->setEndValue(1.0);
        m_anim->start();
    }

    void relayout()
    {
        if (parentWidget() == NULL || m_card == NULL)
            return;

        setGeometry(parentWidget()->rect());

        const int cardWidth = qMin(620, qMax(360, width() - 80));
        const int cardHeight = qMin(460, qMax(240, height() - 120));
        m_card->setGeometry((width() - cardWidth) / 2,
                            qMax(48, (height() - cardHeight) / 3),
                            cardWidth, cardHeight);
    }

    void closePalette()
    {
        if (isVisible() == false)
            return;

        m_search->clearFocus();
        m_anim->stop();
        m_anim->setDuration(motionDuration(140));
        m_anim->setStartValue(m_backdrop);
        m_anim->setEndValue(0.0);
        m_anim->start();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == m_search && event->type() == QEvent::KeyPress)
        {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Down)
            {
                moveSelection(1);
                return true;
            }
            if (keyEvent->key() == Qt::Key_Up)
            {
                moveSelection(-1);
                return true;
            }
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
            {
                triggerCurrent();
                return true;
            }
            if (keyEvent->key() == Qt::Key_Escape)
            {
                closePalette();
                return true;
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    void paintEvent(QPaintEvent *) override
    {
        QColor scrim(0, 0, 0);
        scrim.setAlphaF(0.52 * m_backdrop);
        QPainter painter(this);
        painter.fillRect(rect(), scrim);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (m_card != NULL && m_card->geometry().contains(event->pos()) == false)
            closePalette();
        else
            QWidget::mousePressEvent(event);
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Escape)
        {
            closePalette();
            return;
        }
        QWidget::keyPressEvent(event);
    }

private:
    void rebuildActions()
    {
        m_actions.clear();
        QWidget *root = window();
        if (root == NULL)
            return;

        QSet<QString> seen;
        const QList<QAction *> candidates = root->findChildren<QAction *>();
        for (int i = 0; i < candidates.count(); i++)
        {
            QAction *action = candidates.at(i);
            if (action == NULL || action->isSeparator() == true
                || action->text().isEmpty() == true
                || action->isEnabled() == false)
                continue;

            /* Menu containers with no shortcut are not useful commands */
            if (action->menu() != NULL && action->shortcut().isEmpty() == true)
                continue;

            /* Only commands that belong to the main window, not to any
             * dialog that happens to be a child of it. */
            bool belongs = false;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            const QList<QObject *> objects = action->associatedObjects();
            for (int o = 0; o < objects.count(); o++)
            {
                QWidget *widget = qobject_cast<QWidget *>(objects.at(o));
                if (widget != NULL && widget->window() == root)
                {
                    belongs = true;
                    break;
                }
            }
#else
            const QList<QWidget *> widgets = action->associatedWidgets();
            for (int w = 0; w < widgets.count(); w++)
            {
                if (widgets.at(w) != NULL && widgets.at(w)->window() == root)
                {
                    belongs = true;
                    break;
                }
            }
#endif
            if (belongs == false)
                continue;

            const QString text = cleanText(action->text());
            const QString key = text + "|" + action->shortcut().toString();
            if (seen.contains(key) == true)
                continue;

            seen.insert(key);
            m_actions.append(action);
        }

        std::sort(m_actions.begin(), m_actions.end(),
                  [](QAction *a, QAction *b)
        {
            return cleanText(a->text()).localeAwareCompare(cleanText(b->text())) < 0;
        });
    }

    static QString cleanText(const QString &text)
    {
        QString clean = text;
        clean.remove('&');
        return clean.simplified();
    }

    void filter(const QString &text)
    {
        m_filteredActions.clear();
        m_list->clear();

        const QString needle = text.trimmed().toLower();

        for (int i = 0; i < m_actions.count(); i++)
        {
            QAction *action = m_actions.at(i);
            const QString label = cleanText(action->text());
            const QString shortcut = action->shortcut().toString();

            if (needle.isEmpty() == false
                && label.toLower().contains(needle) == false
                && shortcut.toLower().contains(needle) == false)
                continue;

            QString itemText = label;
            if (shortcut.isEmpty() == false)
                itemText += QString("   %1").arg(shortcut);

            QListWidgetItem *item = new QListWidgetItem(action->icon(), itemText, m_list);
            item->setToolTip(action->toolTip());
            m_filteredActions.append(action);
        }

        if (m_filteredActions.isEmpty())
        {
            QListWidgetItem *item = new QListWidgetItem(tr("No matching commands"), m_list);
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        }
        else
        {
            m_list->setCurrentRow(0);
        }
    }

    void moveSelection(int delta)
    {
        if (m_list->count() == 0)
            return;

        int row = m_list->currentRow() + delta;
        if (row < 0)
            row = m_list->count() - 1;
        else if (row >= m_list->count())
            row = 0;
        m_list->setCurrentRow(row);
    }

    void triggerCurrent()
    {
        const int row = m_list->currentRow();
        if (row < 0 || row >= m_filteredActions.count())
            return;

        QAction *action = m_filteredActions.at(row);
        closePalette();
        if (action != NULL && action->isEnabled() == true)
            QTimer::singleShot(0, action, SLOT(trigger()));
    }

private:
    QFrame *m_card;
    QLineEdit *m_search;
    QListWidget *m_list;
    QLabel *m_hint;
    QGraphicsOpacityEffect *m_cardEffect;
    QVariantAnimation *m_anim;
    QList<QAction *> m_actions;
    QList<QAction *> m_filteredActions;
    qreal m_backdrop;
};

/* ------------------------------------------------------------------ */
/* MainShell                                                           */
/* ------------------------------------------------------------------ */

MainShell::MainShell(QWidget *parent)
    : QWidget(parent)
    , m_tabs(NULL)
    , m_nav(NULL)
    , m_topBar(NULL)
    , m_headerLayout(NULL)
    , m_bodyLayout(NULL)
    , m_commandBar(NULL)
    , m_appMark(NULL)
    , m_workspaceLabel(NULL)
    , m_pageLabel(NULL)
    , m_searchButton(NULL)
    , m_bpmLabel(NULL)
    , m_modeSwitch(NULL)
    , m_modeAction(NULL)
    , m_pageHost(NULL)
    , m_transition(NULL)
    , m_toast(NULL)
    , m_palette(NULL)
    , m_operating(false)
    , m_navigationPosition(Sidebar)
{
    setObjectName("mainShell");

    m_topBar = new QWidget(this);
    m_topBar->setObjectName("topBar");
    m_topBar->setFixedHeight(58);
    m_headerLayout = new QHBoxLayout(m_topBar);
    m_headerLayout->setContentsMargins(12, 0, 12, 0);
    m_headerLayout->setSpacing(10);

    m_appMark = new QLabel(m_topBar);
    m_appMark->setObjectName("appMark");
    m_appMark->setFixedSize(30, 30);
    m_appMark->setAlignment(Qt::AlignCenter);
    m_headerLayout->addWidget(m_appMark);

    QWidget *workspaceBlock = new QWidget(m_topBar);
    QVBoxLayout *workspaceLayout = new QVBoxLayout(workspaceBlock);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->setSpacing(1);

    m_workspaceLabel = new QLabel(tr("New Workspace"), workspaceBlock);
    m_workspaceLabel->setObjectName("workspaceTitle");
    m_workspaceLabel->setFont(uiFont(14, QFont::DemiBold));
    m_workspaceLabel->setMinimumWidth(100);
    m_workspaceLabel->setMaximumWidth(300);
    workspaceLayout->addWidget(m_workspaceLabel);

    m_pageLabel = new QLabel(tr("Workspace"), workspaceBlock);
    m_pageLabel->setObjectName("workspaceSubtitle");
    m_pageLabel->setFont(uiFont(12));
    workspaceLayout->addWidget(m_pageLabel);

    m_headerLayout->addWidget(workspaceBlock);

    m_searchButton = new QToolButton(m_topBar);
    m_searchButton->setObjectName("searchButton");
    m_searchButton->setToolTip(tr("Search commands (Ctrl+Shift+P)"));
    m_searchButton->setIcon(AppIcons::icon("search", 18));
    m_searchButton->setIconSize(QSize(22, 22));
    m_searchButton->setAutoRaise(true);
    connect(m_searchButton, &QToolButton::clicked, this, &MainShell::openCommandPalette);
    m_headerLayout->addWidget(m_searchButton);

    m_bpmLabel = new QLabel(m_topBar);
    m_bpmLabel->setObjectName("bpmBadge");
    m_bpmLabel->setAlignment(Qt::AlignCenter);
    m_bpmLabel->hide();
    m_headerLayout->addWidget(m_bpmLabel);

    m_bodyLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    m_bodyLayout->setContentsMargins(0, 0, 0, 0);
    m_bodyLayout->setSpacing(0);

    m_nav = new NavigationRail(this);
    m_nav->clicked = [this](int index) { setCurrentIndex(index, true); };
    m_nav->doubleClicked = [this](int index) { emit detachRequested(index); };
    m_nav->contextMenuRequested = [this](int index, const QPoint &globalPos)
    {
        QMenu menu(this);
        QAction *detach = menu.addAction(AppIcons::icon("external-link"),
                                         tr("Open in window"));
        QAction *chosen = menu.exec(globalPos);
        if (chosen == detach)
            emit detachRequested(index);
    };
    m_bodyLayout->addWidget(m_nav);

    m_pageHost = new QWidget(this);
    m_pageHost->setObjectName("pageHost");
    m_pageHost->setMinimumSize(0, 0);
    m_pageHost->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    QVBoxLayout *pageLayout = new QVBoxLayout(m_pageHost);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);
    m_bodyLayout->addWidget(m_pageHost, 1);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    /* Pages have their own scroll areas; never let a page's minimum size
     * force the whole application window to grow. */
    mainLayout->setSizeConstraint(QLayout::SetNoConstraint);
    mainLayout->addWidget(m_topBar);
    mainLayout->addLayout(m_bodyLayout, 1);

    QShortcut *paletteShortcut = new QShortcut(QKeySequence("Ctrl+Shift+P"), this);
    connect(paletteShortcut, &QShortcut::activated, this, &MainShell::openCommandPalette);

    QSettings settings;
    const QString navPosition = settings.value("workspace/navposition", "sidebar").toString();
    setNavigationPosition(navPosition == "bottom" ? BottomBar : Sidebar, false);

    refreshTheme();
}

MainShell::~MainShell()
{
}

void MainShell::setPageContainer(QTabWidget *tabs)
{
    if (m_tabs == tabs)
        return;

    m_tabs = tabs;
    if (m_tabs == NULL)
        return;

    m_tabs->setParent(m_pageHost);
    m_pageHost->layout()->addWidget(m_tabs);
    m_tabs->setDocumentMode(true);
    m_tabs->setMinimumSize(0, 0);
    m_tabs->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_tabs->tabBar()->hide();

    connect(m_tabs, &QTabWidget::currentChanged,
            this, &MainShell::slotCurrentChanged);

    rebuildNavigation();
    updateHeader();
}

void MainShell::setCommandBar(QWidget *bar)
{
    if (bar == NULL || m_headerLayout == NULL)
        return;

    m_commandBar = bar;
    if (bar->parentWidget() != m_topBar)
        bar->setParent(m_topBar);

    if (QToolBar *toolBar = qobject_cast<QToolBar *>(bar))
    {
        toolBar->setObjectName("commandBar");
        toolBar->setMovable(false);
        toolBar->setFloatable(false);
        toolBar->setIconSize(QSize(24, 24));
        toolBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        toolBar->setContextMenuPolicy(Qt::PreventContextMenu);
    }

    m_headerLayout->addWidget(bar, 1);
}

void MainShell::setPrimaryActions(QAction *modeAction, QAction *blackoutAction)
{
    if (modeAction != NULL)
    {
        /* Keep the action's shortcut (Ctrl+F12) registered with the window. */
        m_topBar->addAction(modeAction);
        m_modeAction = modeAction;

        m_modeSwitch = new AppSegmentedControl(m_topBar);
        m_modeSwitch->setObjectName("modeSwitch");
        m_modeSwitch->setMinimumWidth(210);
        m_modeSwitch->setFixedHeight(34);
        m_modeSwitch->addSegment(tr("Design"), "pencil-ruler",
                                 Theme::instance()->accent());
        m_modeSwitch->addSegment(tr("Operate"), "play",
                                 Theme::instance()->color(Theme::Success));
        connect(m_modeSwitch, &AppSegmentedControl::currentChanged,
                this, [this](int index)
        {
            if (m_modeAction != NULL && (index == 1) != m_operating)
                m_modeAction->trigger();
        });
        m_headerLayout->addWidget(m_modeSwitch);
    }

    if (blackoutAction != NULL)
    {
        m_topBar->addAction(blackoutAction);

        QToolButton *blackoutButton = new QToolButton(m_topBar);
        blackoutButton->setDefaultAction(blackoutAction);
        blackoutButton->setProperty("uiRole", "danger");
        blackoutButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
        blackoutButton->setIconSize(QSize(24, 24));
        blackoutButton->setObjectName("blackoutButton");
        blackoutButton->setAutoRaise(false);
        blackoutButton->setToolTip(tr("Blackout"));
        m_headerLayout->addWidget(blackoutButton);
    }

    refreshTheme();
    applyModeState();
}

void MainShell::setModeState(bool operate)
{
    m_operating = operate;
    applyModeState();
}

void MainShell::setAudioBpm(double bpm, const QString &title)
{
    if (m_bpmLabel == NULL)
        return;

    if (bpm <= 0)
    {
        m_bpmLabel->hide();
        return;
    }

    m_bpmLabel->setText(tr("BPM %1").arg(QString::number(bpm, 'f', 1)));
    m_bpmLabel->setToolTip(title.isEmpty() ? tr("Current audio tempo")
                                           : tr("%1 — current audio tempo").arg(title));
    m_bpmLabel->show();
}

void MainShell::applyModeState()
{
    if (m_modeSwitch == NULL)
        return;

    Theme *theme = Theme::instance();
    m_modeSwitch->setSegmentColor(0, theme->accent());
    m_modeSwitch->setSegmentColor(1, theme->color(Theme::Success));

    QSignalBlocker blocker(m_modeSwitch);
    m_modeSwitch->setCurrentIndex(m_operating ? 1 : 0, true);
}

void MainShell::setNavigationPosition(NavigationPosition position, bool persist)
{
    m_navigationPosition = position;

    if (m_bodyLayout == NULL || m_nav == NULL || m_pageHost == NULL)
        return;

    m_bodyLayout->removeWidget(m_nav);
    m_bodyLayout->removeWidget(m_pageHost);

    if (position == BottomBar)
    {
        m_nav->setOrientation(Qt::Horizontal);
        m_bodyLayout->setDirection(QBoxLayout::TopToBottom);
        m_bodyLayout->addWidget(m_pageHost, 1);
        m_bodyLayout->addWidget(m_nav);
    }
    else
    {
        m_nav->setOrientation(Qt::Vertical);
        m_bodyLayout->setDirection(QBoxLayout::LeftToRight);
        m_bodyLayout->addWidget(m_nav);
        m_bodyLayout->addWidget(m_pageHost, 1);
    }

    if (persist == true)
    {
        QSettings settings;
        settings.setValue("workspace/navposition",
                          position == BottomBar ? "bottom" : "sidebar");
    }

    update();

    if (m_toast != NULL && m_toast->isVisible() == true)
    {
        const int navOffset = (position == BottomBar && m_nav->isVisible() == true)
                            ? m_nav->height() : 0;
        m_toast->move((width() - m_toast->width()) / 2,
                      height() - m_toast->height() - 24 - navOffset);
    }

    emit navigationPositionChanged(position);
}

MainShell::NavigationPosition MainShell::navigationPosition() const
{
    return m_navigationPosition;
}

void MainShell::setWorkspaceTitle(const QString &title)
{
    if (m_workspaceLabel == NULL)
        return;

    QString text = title;
    const int separator = text.indexOf(" - ");
    if (separator >= 0)
        text = text.mid(separator + 3);

    m_workspaceLabel->setToolTip(text);
    m_workspaceLabel->setText(QFontMetrics(m_workspaceLabel->font())
                              .elidedText(text, Qt::ElideMiddle, 280));
}

void MainShell::setChromeVisible(bool visible)
{
    m_topBar->setVisible(visible);
    m_nav->setVisible(visible);
}

void MainShell::setCurrentIndex(int index, bool animate)
{
    if (m_tabs == NULL || index < 0 || index >= m_tabs->count())
        return;

    if (index == m_tabs->currentIndex())
        return;

    if (animate == false || motionDuration(220) == 0 || m_tabs->currentWidget() == NULL)
    {
        m_tabs->setCurrentIndex(index);
        return;
    }

    const QPixmap previous = m_tabs->currentWidget()->grab();
    m_tabs->setCurrentIndex(index);

    if (previous.isNull() == false)
        startPageTransition(previous);
}

int MainShell::currentIndex() const
{
    return m_tabs != NULL ? m_tabs->currentIndex() : -1;
}

void MainShell::showToast(const QString &text, bool error)
{
    if (m_toast == NULL)
        m_toast = new ToastWidget(this);

    m_toast->showMessage(text, error);

    const int navOffset = (m_navigationPosition == BottomBar && m_nav != NULL
                           && m_nav->isVisible() == true) ? m_nav->height() : 0;
    m_toast->move((width() - m_toast->width()) / 2,
                  height() - m_toast->height() - 24 - navOffset);
}

void MainShell::refreshPages()
{
    rebuildNavigation();
    updateHeader();
}

void MainShell::refreshTheme()
{
    Theme *theme = Theme::instance();

    if (m_appMark != NULL)
    {
        QIcon logo(":/qlcplus.png");
        m_appMark->setPixmap(logo.pixmap(QSize(30, 30)));
    }

    if (m_workspaceLabel != NULL)
        m_workspaceLabel->setFont(uiFont(14, QFont::DemiBold));
    if (m_pageLabel != NULL)
        m_pageLabel->setFont(uiFont(12));

    if (m_searchButton != NULL)
    {
        m_searchButton->setIcon(AppIcons::icon("search", 18));
        m_searchButton->setFixedSize(32, 32);
    }

    if (QToolButton *blackoutButton = findChild<QToolButton *>("blackoutButton"))
    {
        QPalette palette = blackoutButton->palette();
        palette.setColor(QPalette::Window, theme->color(Theme::Surface));
        palette.setColor(QPalette::Button, theme->color(Theme::Surface));
        blackoutButton->setPalette(palette);
    }

    applyModeState();

    if (m_nav != NULL)
        m_nav->update();
    if (m_topBar != NULL)
        m_topBar->update();
    if (m_pageHost != NULL)
        m_pageHost->update();
}

void MainShell::openCommandPalette()
{
    if (m_topBar == NULL || m_topBar->isVisible() == false)
        return;

    if (m_palette == NULL)
        m_palette = new CommandPalette(this);

    m_palette->open();
}

QSize MainShell::sizeHint() const
{
    return QSize(1280, 800);
}

QSize MainShell::minimumSizeHint() const
{
    return QSize(720, 480);
}

void MainShell::closeCommandPalette()
{
    if (m_palette != NULL)
        m_palette->closePalette();
}

void MainShell::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    if (m_transition != NULL)
        m_transition->setGeometry(m_pageHost->rect());

    if (m_toast != NULL && m_toast->isVisible() == true)
    {
        const int navOffset = (m_navigationPosition == BottomBar && m_nav != NULL
                               && m_nav->isVisible() == true) ? m_nav->height() : 0;
        m_toast->move((width() - m_toast->width()) / 2,
                      height() - m_toast->height() - 24 - navOffset);
    }

    if (m_palette != NULL && m_palette->isVisible() == true)
        m_palette->relayout();
}

void MainShell::slotCurrentChanged(int index)
{
    Q_UNUSED(index)

    if (m_nav != NULL)
        m_nav->setActiveIndex(m_tabs->currentIndex(), true);

    updateHeader();
}

void MainShell::rebuildNavigation()
{
    if (m_nav == NULL || m_tabs == NULL)
        return;

    QList<NavigationRail::Item> items;

    for (int i = 0; i < m_tabs->count(); i++)
    {
        NavigationRail::Item item;
        QWidget *page = m_tabs->widget(i);
        item.title = m_tabs->tabText(i);
        item.fallbackIcon = m_tabs->tabIcon(i);
        if (page != NULL)
            item.iconName = page->property("qlcNavIcon").toString();
        items.append(item);
    }

    m_nav->setItems(items);
    m_nav->setActiveIndex(m_tabs->currentIndex(), false);
}

void MainShell::updateHeader()
{
    if (m_pageLabel == NULL || m_tabs == NULL)
        return;

    const int index = m_tabs->currentIndex();
    if (index >= 0 && index < m_tabs->count())
        m_pageLabel->setText(QFontMetrics(m_pageLabel->font())
                             .elidedText(m_tabs->tabText(index), Qt::ElideRight, 280));
    else
        m_pageLabel->setText(tr("Workspace"));
}

void MainShell::startPageTransition(const QPixmap &previous)
{
    if (m_transition != NULL)
    {
        m_transition->deleteLater();
        m_transition = NULL;
    }

    m_transition = new PageTransition(m_pageHost, previous);
    m_transition->setGeometry(m_pageHost->rect());
}
