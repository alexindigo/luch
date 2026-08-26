#pragma once

#include <QObject>
#include <QString>
#include <QVector>

namespace Luch {

struct BrowserEntry {
    QString id;
    QString name;
    QString exec;
    QString icon;
    QString source;
    bool hidden = false;
};

class Config : public QObject
{
    Q_OBJECT

public:
    explicit Config(QObject *parent = nullptr);

    const QVector<BrowserEntry> &browsers() const;
    static QString configPath();

private:
    void load();
    void writeDefaults() const;

    QVector<BrowserEntry> m_browsers;
};

} // namespace Luch
