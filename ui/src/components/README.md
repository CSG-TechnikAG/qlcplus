# QLC+ 4 component library

The modern UI is one design system, not a collection of one-off dialogs.
Stock Qt widgets and the custom controls below share the same theme tokens,
radii, spacing, states and motion rules.

## Design layers

1. **Theme tokens** (`ui/src/theme.cpp`, `ui/src/themes/*.qss`)
   Colors, radii, control heights, arrow/check assets and semantic roles are
   generated for Midnight, Daylight, OLED and Classic.
2. **`UiStyleProxy`**
   Draws command/tool button panels and radio indicators. This keeps hover
   fades, pressed states, ripples and focus rings composited without overlay
   widgets. Button roles are selected with the `uiRole` property:
   `primary`, `danger`, `ghost` (or `quiet`), plus the default secondary style.
3. **Stock widget styles** (`base.qss`)
   Buttons, text inputs, combo boxes, spin boxes, date edits, check boxes,
   sliders, menus, item/list rows, headers, scroll bars and tooltips are styled
   globally. `base.qss` is skipped for Classic, which restores stock widgets.
4. **Custom controls** (`ui/src/components/`)
   The controls below cover the cases where a stock Qt widget is not enough.

## Controls

| Control | Use |
|---|---|
| `AppCard` | Titled/subtitled rounded surface; sections in dialogs and pages. |
| `AppDialog` | Shared dialog shell: icon, title, subtitle, separators, content area and footer actions. |
| `AppEmptyState` | Centered icon tile, title, description and optional primary action. |
| `AppSettingRow` | Title/description text with a control (switch, selector) on the right. |
| `AppSwitch` | Animated on/off switch with theme-aware track and focus ring. |
| `AppSegmentedControl` | Animated exclusive selector with per-segment colors (Design/Operate, Sidebar/Bottom bar). |
| `AppearanceDialog` | Theme settings dialog built from the controls above. |

## Conventions

* All custom controls read `Theme::instance()` and repaint on `Theme::changed`.
* Motion is disabled automatically in Classic or when animated effects are off.
* `AppSegmentedControl::setCurrentIndex()` does not emit `currentChanged()`.
  Only user interaction does, which avoids feedback loops when syncing state.
* Set `uiRole` on a `QPushButton` for variants: `primary`, `danger`, `ghost`.
* Use `AppDialog::contentLayout()` for body content and
  `addFooterButton()` for actions; the standard Close button is supplied.
* Prefer stock `QPushButton`, `QLineEdit`, `QComboBox`, `QMenu`, `QTreeView`
  and `QListWidget` inside pages: they already match the component library.

## Examples

```cpp
AppCard *card = new AppCard(parent);
card->setTitle(tr("Navigation"));
card->setSubtitle(tr("Move the navigation to the side or to the bottom."));

AppSegmentedControl *segments = new AppSegmentedControl(card);
segments->addSegment(tr("Sidebar"), "panel-left");
segments->addSegment(tr("Bottom bar"), "panel-bottom");
segments->setCurrentIndex(1, false);
card->contentLayout()->addWidget(segments);
```

```cpp
AppDialog dialog(tr("Rename fixture"), parent);
dialog.setSubtitle(tr("Choose a name for the selected fixture."));
dialog.setHeaderIcon("fixtures", 24);

QLineEdit *name = new QLineEdit(&dialog);
dialog.contentLayout()->addWidget(name);
QPushButton *rename = dialog.addFooterButton(tr("Rename"), true);
QObject::connect(rename, &QPushButton::clicked, &dialog, &QDialog::accept);
dialog.exec();
```

```cpp
AppEmptyState *empty = new AppEmptyState(parent);
empty->setIcon("fixtures");
empty->setTitle(tr("No fixture selected"));
empty->setDescription(tr("Select a fixture to inspect it, or add a new one."));
empty->setActionText(tr("Add fixture"));
connect(empty, &AppEmptyState::actionTriggered, this, &MyPage::addFixture);
```

## Regression tests

`ui/test/theme/theme_test.cpp` covers theme tokens, palette/contrast, button
effects, legacy icon colour preservation, the segmented control, the switch,
the dialog shell and the empty-state action.

## Page chrome

Workspace pages use the same page-level composition:

* `#pageActionBar` — a rounded card holding page-level actions.
* `QToolBar#pageToolBar` — transparent toolbar inside the action bar.
* `#pagePanel` — a rounded card for the main content/editor surface.
* `#pageCanvas` — the framed Virtual Console canvas.

The Fixture Manager uses `AppSegmentedControl` for its Fixtures / Channel Groups
view switch. Function Manager and Virtual Console use a primary menu button
(`New function`, `Add widget`) with the creation actions inside, plus a compact
secondary toolbar. The page margins are 16 px with 12 px spacing, matching the
dialog and card system.

## Custom canvases

The same theme roles are used by the custom-painted workspaces:

* Virtual Console buttons keep their user-configured background, foreground,
  icon and image, but now draw a rounded panel, state ring and LED from the
  theme. VC selection/resize chrome uses the accent color.
* Virtual Console sliders, cue-list faders, audio-trigger volume and Simple
  Desk channel/playback sliders share the theme-aware slider style.
* Simple Desk channel-group states use theme surfaces; the grand master keeps
  its red semantic fader.
* Show track headers, dividers, time header, cursor-adjacent chrome and the
  timeline background follow the theme; function/item colours remain intact.
* Virtual Console frames, cue-list labels, matrix/XY/Speed Dial preset buttons
  and audio-trigger headers use the shared palette.
