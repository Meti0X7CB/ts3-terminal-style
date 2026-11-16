#include "terminalstyle.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QFontMetrics>
#include <QPainter>
#include <QPushButton>
#include <QStyleFactory>
#include <QStyleOption>
#include <QToolButton>
#include <QEvent>
#include <QAbstractNativeEventFilter>
#include <QDir>
#include <QFile>
#include <QPointer>
#include <QTime>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

namespace {

// Current palette. Filled by applyTheme(); the drawing code reads these directly,
// so switching theme is: applyTheme(), set the app palette, repaint.
QColor kBg, kPanel, kAltBase, kRaised, kSelect, kLine, kBox, kBoxLine,
       kFaint, kMuted, kText, kBright, kDark, kShadow, kAccent;
bool gDark = true;
constexpr int kMenuCheckShift = 10; // extra left gutter in menus with checkable items

void applyTheme(bool dark)
{
    gDark = dark;
    if (dark) {
        kBg = 0x222222;  kPanel = 0x262626;  kAltBase = 0x252525; kRaised = 0x2e2e2e;
        kSelect = 0x383838; kLine = 0x484848; kBox = 0x2c2c2c;   kBoxLine = 0x555555;
        kFaint = 0x555555; kMuted = 0x707070; kText = 0xd4d4d4;   kBright = 0xf0f0f0;
        kDark = 0x1a1a1a;  kShadow = 0x141414;
    } else {
        // cool light grey, slate text, lavender titles (matches the user's light tab theme)
        kBg = 0xe4e5e9;  kPanel = 0xe9eaee;  kAltBase = 0xe1e2e7; kRaised = 0xdadce2;
        kSelect = 0xd0d3dc; kLine = 0xc4c7d0; kBox = 0xd9dbe1;   kBoxLine = 0xb4b8c4;
        kFaint = 0xacb0be; kMuted = 0x8c8fa1; kText = 0x4c4f69;   kBright = 0x303446;
        kDark = 0xbcc0cc;  kShadow = 0x9ca0b0;
    }
    kAccent = dark ? kMuted : QColor(0x72, 0x87, 0xfd);
}

// Windows "Choose your app mode" (Settings > Personalization > Colors)
bool systemIsDark()
{
    // test override: TERMINAL_THEME=light|dark
    const QByteArray force = qgetenv("TERMINAL_THEME");
    if (force == "light") return false;
    if (force == "dark") return true;
#ifdef Q_OS_WIN
    DWORD value = 1, size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER,
                     L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size) == ERROR_SUCCESS)
        return value == 0;
#endif
    return true;
}

// Opt-in diagnostic log: set TERMINAL_STYLE_LOG=1 to write %TEMP%\terminalstyle.log
void logLine(const QString &s)
{
    static const bool on = qEnvironmentVariableIntValue("TERMINAL_STYLE_LOG") != 0;
    if (!on)
        return;
    static bool first = true;
    QFile f(QDir::temp().filePath(QStringLiteral("terminalstyle.log")));
    if (f.open(first ? (QIODevice::WriteOnly | QIODevice::Truncate) : (QIODevice::WriteOnly | QIODevice::Append))) {
        first = false;
        f.write((QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz ")) + s + '\n').toUtf8());
    }
}

QFont monoFont(int pt = 10)
{
    QFont f;
    f.setFamilies({QStringLiteral("Geist Mono"), QStringLiteral("JetBrains Mono"),
                   QStringLiteral("Cascadia Mono"), QStringLiteral("Consolas")});
    f.setStyleHint(QFont::Monospace);
    f.setPointSize(pt);
    return f;
}

QFont fontFor(const QWidget *w) { return w ? w->font() : QApplication::font(); }

bool enabled(const QStyleOption *o) { return o->state & QStyle::State_Enabled; }
bool hovered(const QStyleOption *o) { return enabled(o) && (o->state & QStyle::State_MouseOver); }

// crisp 1px rectangle inside r
void frameRect(QPainter *p, const QRect &r, const QColor &c)
{
    p->save();
    p->setPen(QPen(c, 1));
    p->setBrush(Qt::NoBrush);
    p->drawRect(r.adjusted(0, 0, -1, -1));
    p->restore();
}

enum class Dir { Up, Down, Left, Right };

// small chevron centred in r
void chevron(QPainter *p, const QRect &r, Dir d, const QColor &c, int size = 4)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    QPen pen(c, 1.3);
    pen.setCapStyle(Qt::SquareCap);
    pen.setJoinStyle(Qt::MiterJoin);
    p->setPen(pen);
    const QPointF ctr = QRectF(r).center();
    const qreal s = size, h = size / 2.0;
    QPolygonF poly;
    switch (d) {
    case Dir::Down:  poly << QPointF(ctr.x() - s, ctr.y() - h) << QPointF(ctr.x(), ctr.y() + h) << QPointF(ctr.x() + s, ctr.y() - h); break;
    case Dir::Up:    poly << QPointF(ctr.x() - s, ctr.y() + h) << QPointF(ctr.x(), ctr.y() - h) << QPointF(ctr.x() + s, ctr.y() + h); break;
    case Dir::Right: poly << QPointF(ctr.x() - h, ctr.y() - s) << QPointF(ctr.x() + h, ctr.y()) << QPointF(ctr.x() - h, ctr.y() + s); break;
    case Dir::Left:  poly << QPointF(ctr.x() + h, ctr.y() - s) << QPointF(ctr.x() - h, ctr.y()) << QPointF(ctr.x() + h, ctr.y() + s); break;
    }
    p->drawPolyline(poly);
    p->restore();
}

void glyphText(QPainter *p, const QRect &r, const QString &s, const QColor &c, const QFont &f)
{
    p->save();
    p->setFont(f);
    p->setPen(c);
    p->drawText(r, Qt::AlignCenter, s);
    p->restore();
}

// Ask DWM for a dark caption matching the palette. Windows 10 20H1+ honours
// attribute 20 (dark mode); Windows 11 also honours exact caption/text/border colours.
void darkTitleBar(QWidget *w)
{
#ifdef Q_OS_WIN
    // note: Qt::Popup/ToolTip include the Qt::Window bit, so compare the type, not a mask
    const Qt::WindowType type = w->windowType();
    if (type == Qt::Popup || type == Qt::ToolTip || type == Qt::SplashScreen ||
        type == Qt::Desktop || (w->windowFlags() & Qt::FramelessWindowHint))
        return;
    const HWND hwnd = reinterpret_cast<HWND>(w->winId());
    const BOOL dark = gDark ? TRUE : FALSE;
    HRESULT h1 = DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark));
    if (FAILED(h1)) // Windows 10 before 20H1 used attribute 19
        h1 = DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
    const COLORREF caption = RGB(kBg.red(), kBg.green(), kBg.blue());
    const COLORREF text = RGB(kText.red(), kText.green(), kText.blue());
    const COLORREF border = RGB(kLine.red(), kLine.green(), kLine.blue());
    const HRESULT h2 = DwmSetWindowAttribute(hwnd, 35 /*DWMWA_CAPTION_COLOR*/, &caption, sizeof(caption));
    DwmSetWindowAttribute(hwnd, 36 /*DWMWA_TEXT_COLOR*/, &text, sizeof(text));
    DwmSetWindowAttribute(hwnd, 34 /*DWMWA_BORDER_COLOR*/, &border, sizeof(border));
    logLine(QStringLiteral("titlebar %1 \"%2\" hwnd=%3 dark=%4 hr=%5/%6")
                .arg(QString::fromLatin1(w->metaObject()->className()), w->windowTitle())
                .arg(quintptr(hwnd), 0, 16).arg(gDark)
                .arg(quint32(h1), 0, 16).arg(quint32(h2), 0, 16));
#else
    Q_UNUSED(w)
#endif
}

// Watches for "ImmersiveColorSet" (the Windows light/dark toggle) and re-themes live.
class ThemeWatcher : public QAbstractNativeEventFilter
{
public:
    explicit ThemeWatcher(TerminalStyle *s) : style(s) {}
    bool nativeEventFilter(const QByteArray &type, void *message, long *) override
    {
#ifdef Q_OS_WIN
        if (type == "windows_generic_MSG") {
            const MSG *msg = static_cast<const MSG *>(message);
            if (msg->message == WM_SETTINGCHANGE && msg->lParam &&
                lstrcmpW(reinterpret_cast<LPCWSTR>(msg->lParam), L"ImmersiveColorSet") == 0) {
                const bool dark = systemIsDark();
                if (dark != gDark && style) {
                    QPointer<TerminalStyle> s = style;
                    QTimer::singleShot(0, [s] { if (s) s->retheme(); });
                }
            }
        }
#else
        Q_UNUSED(type) Q_UNUSED(message)
#endif
        return false;
    }
    QPointer<TerminalStyle> style;
};

} // namespace

TerminalStyle::TerminalStyle() : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion")))
{
    setObjectName(QStringLiteral("Terminal"));
    applyTheme(systemIsDark());
    logLine(QStringLiteral("Terminal style loaded, theme=%1").arg(gDark ? "dark" : "light"));
}

TerminalStyle::~TerminalStyle()
{
    if (m_watcher) {
        if (QCoreApplication::instance())
            QCoreApplication::instance()->removeNativeEventFilter(m_watcher);
        delete m_watcher;
    }
}

void TerminalStyle::retheme()
{
    applyTheme(systemIsDark());
    logLine(QStringLiteral("system theme changed -> %1").arg(gDark ? "dark" : "light"));
    QApplication::setPalette(standardPalette());
    // the tui theme uses palette(...) which is resolved when the sheet is applied: re-apply it
    if (auto *app = qobject_cast<QApplication *>(QCoreApplication::instance()))
        if (!app->styleSheet().isEmpty())
            app->setStyleSheet(app->styleSheet());
    for (QWidget *w : QApplication::topLevelWidgets()) {
        if (w->testAttribute(Qt::WA_WState_Created) && w->isVisible())
            darkTitleBar(w);
        w->update();
    }
}

// ---------------------------------------------------------------- palette

QPalette TerminalStyle::standardPalette() const
{
    QPalette pal;
    pal.setColor(QPalette::Window, kBg);
    pal.setColor(QPalette::WindowText, kText);
    pal.setColor(QPalette::Base, kPanel);
    pal.setColor(QPalette::AlternateBase, kAltBase);
    pal.setColor(QPalette::Text, kText);
    pal.setColor(QPalette::Button, kBg);
    pal.setColor(QPalette::ButtonText, kText);
    pal.setColor(QPalette::BrightText, kBright);
    pal.setColor(QPalette::Highlight, kSelect);
    pal.setColor(QPalette::HighlightedText, kBright);
    pal.setColor(QPalette::Link, kBright);
    pal.setColor(QPalette::LinkVisited, kText);
    pal.setColor(QPalette::ToolTipBase, kPanel);
    pal.setColor(QPalette::ToolTipText, kText);
    pal.setColor(QPalette::PlaceholderText, kFaint);
    pal.setColor(QPalette::Light, kRaised);
    pal.setColor(QPalette::Midlight, kLine);
    pal.setColor(QPalette::Mid, kLine);
    pal.setColor(QPalette::Dark, kDark);
    pal.setColor(QPalette::Shadow, kShadow);

    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        pal.setColor(QPalette::Disabled, role, kFaint);
    pal.setColor(QPalette::Disabled, QPalette::Highlight, kRaised);
    pal.setColor(QPalette::Inactive, QPalette::Highlight, kSelect);
    return pal;
}

void TerminalStyle::polish(QPalette &pal) { pal = standardPalette(); }

void TerminalStyle::polish(QApplication *app)
{
    QProxyStyle::polish(app);
    app->setPalette(standardPalette());
    app->setFont(monoFont());
    if (!m_watcher) {
        auto *watcher = new ThemeWatcher(this);
        app->installNativeEventFilter(watcher);
        m_watcher = watcher;
    }
}

void TerminalStyle::polish(QWidget *w)
{
    QProxyStyle::polish(w);
    // hover feedback needs these
    if (qobject_cast<QPushButton *>(w) || qobject_cast<QComboBox *>(w) ||
        qobject_cast<QToolButton *>(w) || qobject_cast<QAbstractItemView *>(w) ||
        w->inherits("QAbstractSpinBox") || w->inherits("QCheckBox") ||
        w->inherits("QRadioButton") || w->inherits("QScrollBar") || w->inherits("QTabBar"))
        w->setAttribute(Qt::WA_Hover, true);

    // TeamSpeak sets its own (small, proportional) fonts after the style loads:
    // force the monospace family everywhere and a 10pt minimum, keep bold/italic.
    QFont f = w->font();
    const QFont mono = monoFont();
    bool changed = false;
    if (f.families().value(0) != mono.families().value(0)) {
        f.setFamilies(mono.families());
        f.setStyleHint(QFont::Monospace);
        changed = true;
    }
    if (f.pointSizeF() > 0 && f.pointSizeF() < 10.0) {
        f.setPointSizeF(10.0);
        changed = true;
    }
    if (changed)
        w->setFont(f);

    // dark title bar for every top-level window (applied once the native window exists)
    if (w->isWindow()) {
        w->installEventFilter(this);
        if (w->testAttribute(Qt::WA_WState_Created))
            darkTitleBar(w);   // window already exists (style switched at runtime)
    }

    // oversized list icons (e.g. the Options sidebar) -> compact 20px
    if (auto *view = qobject_cast<QAbstractItemView *>(w)) {
        if (view->iconSize().height() >= 28)
            view->setIconSize(QSize(20, 20));
    }
}

bool TerminalStyle::eventFilter(QObject *obj, QEvent *ev)
{
    if (ev->type() == QEvent::Show || ev->type() == QEvent::WinIdChange) {
        auto *w = qobject_cast<QWidget *>(obj);
        if (w && w->isWindow() && w->testAttribute(Qt::WA_WState_Created)) {
            darkTitleBar(w);
            // again once the window is actually mapped; DWM can ignore pre-map calls
            QPointer<QWidget> pw = w;
            QTimer::singleShot(50, [pw] { if (pw && pw->isVisible()) darkTitleBar(pw); });
        }
    }
    return QProxyStyle::eventFilter(obj, ev);
}

QSize TerminalStyle::sizeFromContents(ContentsType ct, const QStyleOption *opt,
                                      const QSize &size, const QWidget *w) const
{
    QSize s = QProxyStyle::sizeFromContents(ct, opt, size, w);
    if (ct == CT_ItemViewItem)
        s.rheight() += 6;          // breathing room in the channel tree and lists
    if (ct == CT_MenuItem)
        if (const auto *mi = qstyleoption_cast<const QStyleOptionMenuItem *>(opt))
            if (mi->menuHasCheckableItems)
                s.rwidth() += kMenuCheckShift; // room for our [x] gutter (see CE_MenuItem)
    return s;
}

void TerminalStyle::unpolish(QWidget *w)
{
    if (w->isWindow())
        w->removeEventFilter(this);
    QProxyStyle::unpolish(w);
}

// ---------------------------------------------------------------- metrics

int TerminalStyle::pixelMetric(PixelMetric pm, const QStyleOption *opt, const QWidget *w) const
{
    switch (pm) {
    case PM_IndicatorWidth:
    case PM_ExclusiveIndicatorWidth:
        return QFontMetrics(fontFor(w)).horizontalAdvance(QStringLiteral("[x]"));
    case PM_IndicatorHeight:
    case PM_ExclusiveIndicatorHeight:
        return QFontMetrics(fontFor(w)).height();
    case PM_CheckBoxLabelSpacing:
    case PM_RadioButtonLabelSpacing:
        return QFontMetrics(fontFor(w)).horizontalAdvance(QLatin1Char(' '));
    case PM_ScrollBarExtent:      return 8;
    case PM_ScrollBarSliderMin:   return 24;
    case PM_DefaultFrameWidth:    return 1;
    case PM_ComboBoxFrameWidth:   return 2;
    case PM_SpinBoxFrameWidth:    return 2;
    case PM_ButtonShiftHorizontal:
    case PM_ButtonShiftVertical:
    case PM_TabBarTabShiftVertical:
    case PM_TabBarTabShiftHorizontal:
        return 0;
    case PM_SplitterWidth:        return 3;
    case PM_MenuPanelWidth:       return 1;
    case PM_MenuBarPanelWidth:    return 0;
    case PM_ToolBarFrameWidth:    return 0;
    case PM_ToolBarHandleExtent:  return 6;
    case PM_SliderThickness:      return 14;
    case PM_SliderLength:         return 6;
    case PM_FocusFrameHMargin:
    case PM_FocusFrameVMargin:    return 0;
    default:
        return QProxyStyle::pixelMetric(pm, opt, w);
    }
}

int TerminalStyle::styleHint(StyleHint sh, const QStyleOption *opt, const QWidget *w,
                             QStyleHintReturn *ret) const
{
    switch (sh) {
    case SH_ComboBox_Popup:                    return 0;  // plain list, no macOS-style popup
    case SH_EtchDisabledText:
    case SH_DitherDisabledText:                return 0;
    case SH_ItemView_ShowDecorationSelected:   return 1;
    case SH_ScrollBar_Transient:               return 0;
    case SH_Menu_Scrollable:                   return 1;
    case SH_UnderlineShortcut:                 return 0;
    default:
        return QProxyStyle::styleHint(sh, opt, w, ret);
    }
}

// ---------------------------------------------------------------- primitives

void TerminalStyle::drawPrimitive(PrimitiveElement pe, const QStyleOption *opt, QPainter *p,
                                  const QWidget *w) const
{
    const QRect r = opt->rect;
    switch (pe) {

    case PE_IndicatorCheckBox: {
        QString s = QStringLiteral("[ ]");
        if (opt->state & State_On) s = QStringLiteral("[x]");
        else if (opt->state & State_NoChange) s = QStringLiteral("[-]");
        const QColor c = !enabled(opt) ? kFaint
                       : (hovered(opt) || (opt->state & State_On)) ? kBright : kMuted;
        glyphText(p, r, s, c, fontFor(w));
        return;
    }
    case PE_IndicatorRadioButton: {
        const QString s = (opt->state & State_On) ? QStringLiteral("(*)") : QStringLiteral("( )");
        const QColor c = !enabled(opt) ? kFaint
                       : (hovered(opt) || (opt->state & State_On)) ? kBright : kMuted;
        glyphText(p, r, s, c, fontFor(w));
        return;
    }
    case PE_IndicatorItemViewItemCheck: {
        // checkable rows in lists/trees: same [x] look, drawn a little smaller
        QFont f = fontFor(w);
        QString s = (opt->state & State_On) ? QStringLiteral("[x]")
                  : (opt->state & State_NoChange) ? QStringLiteral("[-]") : QStringLiteral("[ ]");
        glyphText(p, r.adjusted(-6, 0, 6, 0), s, (opt->state & State_On) ? kBright : kMuted, f);
        return;
    }

    case PE_IndicatorArrowDown:  chevron(p, r, Dir::Down,  enabled(opt) ? kText : kFaint); return;
    case PE_IndicatorArrowUp:    chevron(p, r, Dir::Up,    enabled(opt) ? kText : kFaint); return;
    case PE_IndicatorArrowLeft:  chevron(p, r, Dir::Left,  enabled(opt) ? kText : kFaint); return;
    case PE_IndicatorArrowRight: chevron(p, r, Dir::Right, enabled(opt) ? kText : kFaint); return;
    case PE_IndicatorSpinUp:
    case PE_IndicatorSpinPlus:   chevron(p, r, Dir::Up,   enabled(opt) ? kText : kFaint, 3); return;
    case PE_IndicatorSpinDown:
    case PE_IndicatorSpinMinus:  chevron(p, r, Dir::Down, enabled(opt) ? kText : kFaint, 3); return;
    case PE_IndicatorHeaderArrow:
        if (const auto *h = qstyleoption_cast<const QStyleOptionHeader *>(opt))
            chevron(p, r, h->sortIndicator == QStyleOptionHeader::SortUp ? Dir::Up : Dir::Down, kMuted, 3);
        return;

    case PE_IndicatorBranch:
        if (opt->state & State_Children)
            chevron(p, r, (opt->state & State_Open) ? Dir::Down : Dir::Right,
                    hovered(opt) ? kBright : kMuted, 3);
        return; // no dotted tree lines

    case PE_PanelItemViewItem:
    case PE_PanelItemViewRow: {
        if (opt->state & State_Selected) {
            p->fillRect(r, kSelect);
        } else if (hovered(opt)) {
            p->fillRect(r, kRaised);
        } else if (pe == PE_PanelItemViewRow) {
            if (const auto *v = qstyleoption_cast<const QStyleOptionViewItem *>(opt))
                if (v->features & QStyleOptionViewItem::Alternate)
                    p->fillRect(r, opt->palette.alternateBase());
        }
        return;
    }

    case PE_PanelButtonCommand: {
        QColor bg = kBg, border = kLine;
        if (opt->state & (State_Sunken | State_On)) { bg = kSelect; border = kMuted; }
        else if (hovered(opt)) { bg = kRaised; border = kMuted; }
        p->fillRect(r, bg);
        frameRect(p, r, enabled(opt) ? border : kRaised);
        return;
    }
    case PE_FrameDefaultButton:
        return;
    case PE_PanelButtonTool: {
        if (opt->state & (State_Sunken | State_On)) {
            p->fillRect(r, kSelect);
            frameRect(p, r, kMuted);
        } else if (hovered(opt) && (opt->state & State_AutoRaise)) {
            p->fillRect(r, kRaised);
            frameRect(p, r, kLine);
        } else if (!(opt->state & State_AutoRaise)) {
            frameRect(p, r, kLine);
        }
        return;
    }

    case PE_PanelLineEdit: {
        if (w && w->parentWidget() && (w->parentWidget()->inherits("QAbstractSpinBox") ||
                                        w->parentWidget()->inherits("QComboBox")))
            return; // the container draws the frame
        p->fillRect(r, opt->palette.base());
        frameRect(p, r, (opt->state & State_HasFocus) ? kMuted : kLine);
        return;
    }
    case PE_FrameLineEdit:
        frameRect(p, r, (opt->state & State_HasFocus) ? kMuted : kLine);
        return;

    case PE_Frame:
    case PE_FrameWindow:
    case PE_FrameTabWidget:
    case PE_FrameDockWidget:
    case PE_FrameStatusBarItem:
    case PE_FrameGroupBox:
        if (pe == PE_FrameStatusBarItem) return;
        frameRect(p, r, kLine);
        return;

    case PE_FrameMenu:
        frameRect(p, r, kLine);
        return;
    case PE_PanelMenu:
        p->fillRect(r, kPanel);
        frameRect(p, r, kLine);
        return;
    case PE_PanelTipLabel:
        p->fillRect(r, kPanel);
        frameRect(p, r, kLine);
        return;

    case PE_FrameFocusRect:
        return; // focus is shown by brighter borders instead

    case PE_PanelMenuBar:
    case PE_PanelToolBar:
    case PE_PanelStatusBar:
        p->fillRect(r, kBg);
        return;
    case PE_IndicatorToolBarSeparator:
        p->fillRect((opt->state & State_Horizontal)
                        ? QRect(r.center().x(), r.top() + 3, 1, r.height() - 6)
                        : QRect(r.left() + 3, r.center().y(), r.width() - 6, 1),
                    kLine);
        return;
    case PE_IndicatorToolBarHandle:
        return;

    case PE_IndicatorTabClose:
        glyphText(p, r, QStringLiteral("x"), hovered(opt) ? kBright : kMuted, fontFor(w));
        return;

    default:
        QProxyStyle::drawPrimitive(pe, opt, p, w);
    }
}

// ---------------------------------------------------------------- controls

void TerminalStyle::drawControl(ControlElement ce, const QStyleOption *opt, QPainter *p,
                                const QWidget *w) const
{
    const QRect r = opt->rect;
    switch (ce) {

    case CE_ProgressBarGroove:
        p->fillRect(r, kBg);
        frameRect(p, r, kLine);
        return;

    case CE_ProgressBarContents:
        if (const auto *pb = qstyleoption_cast<const QStyleOptionProgressBar *>(opt)) {
            // [█████░░░░]  as discrete cells
            const QRect in = r.adjusted(3, 3, -3, -3);
            const int cell = 6, gap = 2;
            const int cells = qMax(1, (in.width() + gap) / (cell + gap));
            const qint64 range = qint64(pb->maximum) - pb->minimum;
            // range 0 = "busy": no fill (TeamSpeak doesn't use busy bars)
            const int filled = range > 0
                ? int(qRound64(double(pb->progress - pb->minimum) / double(range) * cells)) : 0;
            for (int i = 0; i < cells; ++i) {
                const QRect c(in.left() + i * (cell + gap), in.top(), cell, in.height());
                p->fillRect(c, i < filled ? kText : kRaised);
            }
        }
        return;

    case CE_ProgressBarLabel:
        return; // the cells are the label; TS shows numbers elsewhere

    case CE_TabBarTabShape:
        {
            if (opt->state & State_Selected) {
                p->fillRect(r, kPanel);
                p->fillRect(QRect(r.left(), r.top(), r.width(), 1), kText);
            } else if (hovered(opt)) {
                p->fillRect(r, kRaised);
            }
        }
        return;

    case CE_TabBarTabLabel:
        if (const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(opt)) {
            QStyleOptionTab t(*tab);
            t.palette.setColor(QPalette::WindowText, (tab->state & State_Selected) ? kBright
                                                     : hovered(opt) ? kText : kMuted);
            QProxyStyle::drawControl(ce, &t, p, w);
        }
        return;

    case CE_HeaderSection:
        p->fillRect(r, kBg);
        p->fillRect(QRect(r.left(), r.bottom(), r.width(), 1), kLine);
        p->fillRect(QRect(r.right(), r.top() + 3, 1, r.height() - 6), kLine);
        return;
    case CE_HeaderLabel:
        if (const auto *h = qstyleoption_cast<const QStyleOptionHeader *>(opt)) {
            QStyleOptionHeader hh(*h);
            hh.palette.setColor(QPalette::ButtonText, kMuted);
            QProxyStyle::drawControl(ce, &hh, p, w);
        }
        return;
    case CE_HeaderEmptyArea:
        p->fillRect(r, kBg);
        p->fillRect(QRect(r.left(), r.bottom(), r.width(), 1), kLine);
        return;

    case CE_Splitter:
        p->fillRect(r, kBg);
        if (opt->state & State_Horizontal)
            p->fillRect(QRect(r.center().x(), r.top(), 1, r.height()), hovered(opt) ? kMuted : kLine);
        else
            p->fillRect(QRect(r.left(), r.center().y(), r.width(), 1), hovered(opt) ? kMuted : kLine);
        return;

    case CE_ToolBar:
        p->fillRect(r, kBg);
        p->fillRect(QRect(r.left(), r.bottom(), r.width(), 1), kLine);
        return;

    case CE_MenuBarEmptyArea:
        p->fillRect(r, kBg);
        return;
    case CE_MenuBarItem:
        if (const auto *mi = qstyleoption_cast<const QStyleOptionMenuItem *>(opt)) {
            p->fillRect(r, (opt->state & (State_Selected | State_Sunken)) ? kSelect : kBg);
            QStyleOptionMenuItem m(*mi);
            m.state &= ~(State_Selected | State_Sunken); // we drew the highlight
            m.palette.setColor(QPalette::ButtonText,
                               (opt->state & State_Selected) ? kBright : kText);
            QCommonStyle::drawControl(ce, &m, p, w);
        }
        return;

    case CE_MenuItem:
        if (const auto *mi = qstyleoption_cast<const QStyleOptionMenuItem *>(opt)) {
            if (mi->menuItemType == QStyleOptionMenuItem::Separator) {
                p->fillRect(QRect(r.left() + 4, r.center().y(), r.width() - 8, 1), kLine);
                return;
            }
            QStyleOptionMenuItem m(*mi);
            if (!m.menuHasCheckableItems) {
                QProxyStyle::drawControl(ce, &m, p, w);
                return;
            }
            // menus with checkable items: draw [x] / (*) ourselves in a left gutter
            // and shift Fusion's rendering right so the text never touches it
            if (m.state & State_Selected)
                p->fillRect(r, kSelect);
            const bool checkable = m.checkType != QStyleOptionMenuItem::NotCheckable;
            const bool on = m.checked;
            const bool excl = m.checkType == QStyleOptionMenuItem::Exclusive;
            m.checkType = QStyleOptionMenuItem::NotCheckable;
            m.checked = false;
            m.rect = r.adjusted(kMenuCheckShift, 0, 0, 0);
            QProxyStyle::drawControl(ce, &m, p, w);
            if (checkable && m.icon.isNull()) {
                QRect cr(r.left() + 6, r.top(),
                         QFontMetrics(m.font).horizontalAdvance(QStringLiteral("[x]")), r.height());
                glyphText(p, cr, excl ? (on ? "(*)" : "( )") : (on ? "[x]" : "[ ]"),
                          on ? kBright : kMuted, m.font);
            }
        }
        return;

    case CE_ShapedFrame:
        if (const auto *f = qstyleoption_cast<const QStyleOptionFrame *>(opt)) {
            if (f->frameShape == QFrame::NoFrame) return;
            if (f->frameShape == QFrame::HLine) {
                p->fillRect(QRect(r.left(), r.center().y(), r.width(), 1), kLine);
                return;
            }
            if (f->frameShape == QFrame::VLine) {
                p->fillRect(QRect(r.center().x(), r.top(), 1, r.height()), kLine);
                return;
            }
            frameRect(p, r, kLine);
        }
        return;

    default:
        QProxyStyle::drawControl(ce, opt, p, w);
    }
}

// ---------------------------------------------------------------- complex controls

QRect TerminalStyle::subControlRect(ComplexControl cc, const QStyleOptionComplex *opt,
                                    SubControl sc, const QWidget *w) const
{
    QRect r = QProxyStyle::subControlRect(cc, opt, sc, w);
    if (cc == CC_GroupBox) {
        // put the title *on* the top border:  ┌─ title ──────┐
        const auto *gb = qstyleoption_cast<const QStyleOptionGroupBox *>(opt);
        if (gb && !gb->text.isEmpty()) {
            const QFontMetrics fm(fontFor(w));
            if (sc == SC_GroupBoxFrame) {
                r = opt->rect;
                r.setTop(opt->rect.top() + fm.height() / 2);
            } else if (sc == SC_GroupBoxLabel) {
                const int x = opt->rect.left() + 10 +
                              ((gb->subControls & SC_GroupBoxCheckBox)
                                   ? pixelMetric(PM_IndicatorWidth, opt, w) + 4 : 0);
                r = QRect(x, opt->rect.top(), fm.horizontalAdvance(gb->text) + 8, fm.height());
            } else if (sc == SC_GroupBoxCheckBox) {
                r = QRect(opt->rect.left() + 10, opt->rect.top(),
                          pixelMetric(PM_IndicatorWidth, opt, w), fm.height());
            } else if (sc == SC_GroupBoxContents) {
                r = opt->rect.adjusted(6, fm.height() + 4, -6, -6);
            }
        }
    }
    return r;
}

void TerminalStyle::drawComplexControl(ComplexControl cc, const QStyleOptionComplex *opt,
                                       QPainter *p, const QWidget *w) const
{
    const QRect r = opt->rect;
    switch (cc) {

    case CC_GroupBox:
        if (const auto *gb = qstyleoption_cast<const QStyleOptionGroupBox *>(opt)) {
            const QRect frame = subControlRect(cc, opt, SC_GroupBoxFrame, w);
            if (!(gb->features & QStyleOptionFrame::Flat))
                frameRect(p, frame, kLine);
            else
                p->fillRect(QRect(frame.left(), frame.top(), frame.width(), 1), kLine);

            if (!gb->text.isEmpty()) {
                const QRect label = subControlRect(cc, opt, SC_GroupBoxLabel, w);
                QRect gap = label;
                if (gb->subControls & SC_GroupBoxCheckBox)
                    gap.setLeft(subControlRect(cc, opt, SC_GroupBoxCheckBox, w).left() - 4);
                p->fillRect(gap.adjusted(-4, 0, 0, 0), kBg);
                p->save();
                p->setFont(fontFor(w));
                p->setPen(enabled(opt) ? kAccent : kFaint);
                p->drawText(label, Qt::AlignCenter, gb->text);
                p->restore();
            }
            if (gb->subControls & SC_GroupBoxCheckBox) {
                QStyleOptionButton box;
                box.QStyleOption::operator=(*gb);
                box.rect = subControlRect(cc, opt, SC_GroupBoxCheckBox, w);
                drawPrimitive(PE_IndicatorCheckBox, &box, p, w);
            }
        }
        return;

    case CC_ComboBox:
        if (const auto *cb = qstyleoption_cast<const QStyleOptionComboBox *>(opt)) {
            const bool hot = hovered(opt) || (opt->state & (State_HasFocus | State_On));
            if (cb->frame) {
                p->fillRect(r, kBox);
                frameRect(p, r, hot ? kMuted : kBoxLine);
            }
            const QRect arrow = subControlRect(cc, opt, SC_ComboBoxArrow, w);
            p->fillRect(QRect(arrow.left(), r.top() + 3, 1, r.height() - 6), kLine);
            chevron(p, arrow, Dir::Down, hot ? kBright : kMuted, 3);
        }
        return;

    case CC_SpinBox:
        if (const auto *sb = qstyleoption_cast<const QStyleOptionSpinBox *>(opt)) {
            const bool hot = hovered(opt) || (opt->state & State_HasFocus);
            if (sb->frame) {
                p->fillRect(r, kBox);
                frameRect(p, r, hot ? kMuted : kBoxLine);
            }
            if (sb->buttonSymbols != QAbstractSpinBox::NoButtons) {
                const QRect up = subControlRect(cc, opt, SC_SpinBoxUp, w);
                const QRect dn = subControlRect(cc, opt, SC_SpinBoxDown, w);
                p->fillRect(QRect(up.left() - 1, r.top() + 3, 1, r.height() - 6), kLine);
                const bool upHot = (sb->activeSubControls & SC_SpinBoxUp) && hovered(opt);
                const bool dnHot = (sb->activeSubControls & SC_SpinBoxDown) && hovered(opt);
                chevron(p, up, Dir::Up,
                        (sb->stepEnabled & QAbstractSpinBox::StepUpEnabled) ? (upHot ? kBright : kMuted) : kFaint, 3);
                chevron(p, dn, Dir::Down,
                        (sb->stepEnabled & QAbstractSpinBox::StepDownEnabled) ? (dnHot ? kBright : kMuted) : kFaint, 3);
            }
        }
        return;

    case CC_ScrollBar:
        if (const auto *sb = qstyleoption_cast<const QStyleOptionSlider *>(opt)) {
            p->fillRect(r, kBg);
            const QRect slider = subControlRect(cc, opt, SC_ScrollBarSlider, w);
            const bool hot = (sb->activeSubControls & SC_ScrollBarSlider) &&
                             (hovered(opt) || (opt->state & State_Sunken));
            QRect s = slider;
            if (sb->orientation == Qt::Vertical) s.adjust(2, 0, -2, 0);
            else s.adjust(0, 2, 0, -2);
            p->fillRect(s, hot ? kMuted : kLine);
        }
        return;

    case CC_Slider:
        if (const auto *sl = qstyleoption_cast<const QStyleOptionSlider *>(opt)) {
            const QRect groove = subControlRect(cc, opt, SC_SliderGroove, w);
            const QRect handle = subControlRect(cc, opt, SC_SliderHandle, w);
            const bool horiz = sl->orientation == Qt::Horizontal;
            if (horiz) {
                const int y = groove.center().y();
                p->fillRect(QRect(groove.left(), y, groove.width(), 1), kLine);
                p->fillRect(QRect(groove.left(), y, handle.center().x() - groove.left(), 1), kMuted);
                p->fillRect(QRect(handle.center().x() - 2, handle.top() + 1, 5, handle.height() - 2),
                            hovered(opt) || (opt->state & State_Sunken) ? kBright : kText);
            } else {
                const int x = groove.center().x();
                p->fillRect(QRect(x, groove.top(), 1, groove.height()), kLine);
                p->fillRect(QRect(x, handle.center().y(), 1, groove.bottom() - handle.center().y()), kMuted);
                p->fillRect(QRect(handle.left() + 1, handle.center().y() - 2, handle.width() - 2, 5),
                            hovered(opt) || (opt->state & State_Sunken) ? kBright : kText);
            }
        }
        return;

    default:
        QProxyStyle::drawComplexControl(cc, opt, p, w);
    }
}
