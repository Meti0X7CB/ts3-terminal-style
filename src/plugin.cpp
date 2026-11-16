#include <QStylePlugin>
#include "terminalstyle.h"

class TerminalStylePlugin : public QStylePlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QStyleFactoryInterface_iid FILE "terminalstyle.json")
public:
    QStyle *create(const QString &key) override
    {
        if (key.compare(QLatin1String("terminal"), Qt::CaseInsensitive) == 0)
            return new TerminalStyle;
        return nullptr;
    }
};

#include "plugin.moc"
