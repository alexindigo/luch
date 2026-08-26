#pragma once

#include <QAbstractListModel>
#include <QSet>
#include <QVector>

#include "config.h"

namespace Luch {

class BrowserRegistry : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole,
        NameRole,
        IconNameRole,
        ExecRole,
        ShortcutHintRole,
    };
    Q_ENUM(Role)

    explicit BrowserRegistry(Config *config, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QString execAt(int row) const;

private:
    struct Item {
        QString id;
        QString name;
        QString iconName;
        QString exec;
    };

    void rebuild();
    void scanDesktopEntries(QSet<QString> &seenIds);
    static bool parseDesktopEntry(const QString &path, Item &item,
                                  bool &handlesHttp);

    Config *m_config = nullptr;
    QVector<Item> m_items;
};

} // namespace Luch
