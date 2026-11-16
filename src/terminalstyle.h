#pragma once
#include <QProxyStyle>

class QAbstractNativeEventFilter;

// A flat, monospace, TUI-looking Qt style built on top of Fusion.
// Checkboxes draw as [x] / [ ], radios as (*) / ( ), group boxes as
// line frames with the title set into the top border, progress bars as cells.
class TerminalStyle : public QProxyStyle
{
    Q_OBJECT
public:
    TerminalStyle();
    ~TerminalStyle() override;

    // re-read the Windows light/dark setting and repaint everything
    void retheme();

    void polish(QPalette &pal) override;
    void polish(QApplication *app) override;
    void polish(QWidget *w) override;
    void unpolish(QWidget *w) override;
    QPalette standardPalette() const override;

    void drawPrimitive(PrimitiveElement pe, const QStyleOption *opt, QPainter *p,
                       const QWidget *w = nullptr) const override;
    void drawControl(ControlElement ce, const QStyleOption *opt, QPainter *p,
                     const QWidget *w = nullptr) const override;
    void drawComplexControl(ComplexControl cc, const QStyleOptionComplex *opt, QPainter *p,
                            const QWidget *w = nullptr) const override;
    QRect subControlRect(ComplexControl cc, const QStyleOptionComplex *opt, SubControl sc,
                         const QWidget *w = nullptr) const override;
    bool eventFilter(QObject *obj, QEvent *ev) override;
    QSize sizeFromContents(ContentsType ct, const QStyleOption *opt, const QSize &size,
                           const QWidget *w = nullptr) const override;
    int pixelMetric(PixelMetric pm, const QStyleOption *opt = nullptr,
                    const QWidget *w = nullptr) const override;
    int styleHint(StyleHint sh, const QStyleOption *opt = nullptr, const QWidget *w = nullptr,
                  QStyleHintReturn *ret = nullptr) const override;

private:
    QAbstractNativeEventFilter *m_watcher = nullptr;
};
