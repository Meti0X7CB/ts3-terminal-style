// Loads the "Terminal" style through the normal plugin mechanism (like TeamSpeak does)
// and renders a sample of widgets to gallery.png.
// usage: gallery <plugin-root-dir> <out.png>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QStyleFactory>
#include <QTabWidget>
#include <QTextStream>
#include <windows.h>
#include <dwmapi.h>
#include <QTreeWidget>
#include <QVBoxLayout>

int main(int argc, char **argv)
{
    if (argc > 1) QCoreApplication::addLibraryPath(QString::fromLocal8Bit(argv[1]));
    QApplication app(argc, argv);
    QTextStream out(stdout);
    out << "styles: " << QStyleFactory::keys().join(", ") << "\n";
    QStyle *st = QStyleFactory::create("Terminal");
    if (!st) { out << "Terminal style NOT found\n"; return 1; }
    QApplication::setStyle(st);

    QWidget win;
    win.resize(900, 560);
    auto *root = new QVBoxLayout(&win);

    auto *mb = new QMenuBar;
    mb->addMenu("Connections"); mb->addMenu("Bookmarks"); mb->addMenu("Self"); mb->addMenu("Tools");
    root->setMenuBar(mb);

    auto *row = new QHBoxLayout; root->addLayout(row);

    // left: channel tree
    auto *tree = new QTreeWidget; tree->setHeaderHidden(true);
    auto *srv = new QTreeWidgetItem(tree, {"My Server"});
    auto *home = new QTreeWidgetItem(srv, {"home"});
    new QTreeWidgetItem(home, {"you"});
    auto *chill = new QTreeWidgetItem(srv, {"Music"});
    new QTreeWidgetItem(chill, {"alex"}); new QTreeWidgetItem(chill, {"sam"});
    new QTreeWidgetItem(srv, {"Gaming 1"});
    tree->expandAll(); tree->setCurrentItem(home->child(0));
    tree->setMinimumWidth(260);
    row->addWidget(tree);

    // right: options-like controls
    auto *col = new QVBoxLayout; row->addLayout(col);
    auto *gb = new QGroupBox("channel tree");
    auto *g = new QVBoxLayout(gb);
    auto *c1 = new QCheckBox("expand all channels on login"); c1->setChecked(true);
    g->addWidget(c1);
    g->addWidget(new QCheckBox("sort clients below channels"));
    auto *c3 = new QCheckBox("show tooltip info on hover"); c3->setChecked(true); g->addWidget(c3);
    auto *r1 = new QRadioButton("hourly"); r1->setChecked(true);
    auto *rr = new QHBoxLayout; rr->addWidget(r1); rr->addWidget(new QRadioButton("daily")); rr->addStretch();
    g->addLayout(rr);
    col->addWidget(gb);

    auto *gb2 = new QGroupBox("design");
    auto *g2 = new QVBoxLayout(gb2);
    auto *combo = new QComboBox; combo->addItems({"terminal", "default"});
    g2->addWidget(combo);
    auto *spin = new QSpinBox; spin->setMaximum(900); spin->setValue(400); g2->addWidget(spin);
    auto *le = new QLineEdit; le->setPlaceholderText("enter chat message..."); g2->addWidget(le);
    auto *sl = new QSlider(Qt::Horizontal); sl->setValue(70); g2->addWidget(sl);
    auto *pb = new QProgressBar; pb->setValue(62); g2->addWidget(pb);
    col->addWidget(gb2);

    auto *btns = new QHBoxLayout; btns->addStretch();
    auto *ok = new QPushButton("ok"); ok->setDefault(true);
    btns->addWidget(ok); btns->addWidget(new QPushButton("cancel"));
    auto *ap = new QPushButton("apply"); ap->setEnabled(false); btns->addWidget(ap);
    col->addLayout(btns);

    auto *tabs = new QTabWidget;
    tabs->addTab(new QListWidget, "My Server"); tabs->addTab(new QWidget, "home");
    tabs->setMaximumHeight(110);
    root->addWidget(tabs);

    win.show();
    app.processEvents();
    const QString outPath = argc > 2 ? QString::fromLocal8Bit(argv[2]) : "gallery.png";
    win.grab().save(outPath);

    // a menu with checkable items
    QMenu menu;
    menu.addAction("connect");
    auto *a = menu.addAction("show away message"); a->setCheckable(true); a->setChecked(true);
    auto *b2 = menu.addAction("minimize to tray"); b2->setCheckable(true);
    menu.addSeparator(); menu.addAction("quit");
    menu.ensurePolished(); menu.adjustSize();
    menu.grab().save(QString(outPath).replace(".png", "_menu.png"));

    // read back what DWM thinks about the window's title bar
    {
        app.processEvents();
        BOOL dark = 2; COLORREF cap = 0;
        HWND h = reinterpret_cast<HWND>(win.winId());
        HRESULT a = DwmGetWindowAttribute(h, 20, &dark, sizeof(dark));
        HRESULT b = DwmGetWindowAttribute(h, 35, &cap, sizeof(cap));
        out << "dwm darkmode=" << dark << " hr=" << Qt::hex << quint32(a)
            << " caption=" << quint32(cap) << " hr=" << quint32(b) << Qt::dec << "\n";
    }
    out << "saved " << outPath << "\n";
    out.flush();

    return 0;
}
