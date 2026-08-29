#include "browserregistry.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>

namespace Luch {

BrowserRegistry::BrowserRegistry(Config *config,
                                 const QStringList &mimeMatches,
                                 QObject *parent)
    : QAbstractListModel(parent)
    , m_config(config)
    , m_mimeMatches(mimeMatches)
{
    rebuild();
}

int BrowserRegistry::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant BrowserRegistry::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};

    const Item &item = m_items.at(index.row());
    switch (role) {
    case IdRole:
        return item.id;
    case NameRole:
        return item.name;
    case IconNameRole:
        return item.iconName;
    case ExecRole:
        return item.exec;
    case ShortcutHintRole:
        return index.row() < 9 ? QString::number(index.row() + 1) : QString();
    default:
        return {};
    }
}

QHash<int, QByteArray> BrowserRegistry::roleNames() const
{
    return {
        {IdRole, "id"},
        {NameRole, "name"},
        {IconNameRole, "iconName"},
        {ExecRole, "exec"},
        {ShortcutHintRole, "shortcutHint"},
    };
}

QString BrowserRegistry::execAt(int row) const
{
    return (row >= 0 && row < m_items.size()) ? m_items.at(row).exec
                                              : QString();
}

QString BrowserRegistry::idAt(int row) const
{
    return (row >= 0 && row < m_items.size()) ? m_items.at(row).id
                                              : QString();
}

void BrowserRegistry::setMimeMatches(const QStringList &mimeMatches)
{
    if (mimeMatches == m_mimeMatches)
        return;
    m_mimeMatches = mimeMatches;
    rebuild();
}

void BrowserRegistry::rebuild()
{
    beginResetModel();
    m_items.clear();

    QSet<QString> seenIds;
    scanDesktopEntries(seenIds);

    for (const BrowserEntry &entry : m_config->browsers()) {
        if (entry.source != QLatin1String("manual"))
            continue;
        if (entry.hidden)
            continue;
        if (seenIds.contains(entry.id))
            continue;
        seenIds.insert(entry.id);
        m_items.append({entry.id, entry.name, entry.icon, entry.exec});
    }

    endResetModel();
}

void BrowserRegistry::scanDesktopEntries(QSet<QString> &seenIds)
{
    QSet<QString> hiddenSet;
    for (const BrowserEntry &entry : m_config->browsers()) {
        if (entry.hidden)
            hiddenSet.insert(entry.id);
    }

    QStringList dirs;
    const QString localDir =
        QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    dirs << localDir;
    for (const QString &dir :
         QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation)) {
        if (dir != localDir && !dirs.contains(dir))
            dirs << dir;
    }

    for (const QString &dirPath : dirs) {
        const QDir dir(dirPath);
        const QStringList files = dir.entryList({QStringLiteral("*.desktop")},
                                                QDir::Files, QDir::Name);
        for (const QString &file : files) {
            const QString id = file.chopped(8); // strip ".desktop"
            if (seenIds.contains(id))
                continue;

            Item item;
            bool handlesTarget = false;
            if (!parseDesktopEntry(dir.absoluteFilePath(file), item,
                                   m_mimeMatches, handlesTarget))
                continue;
            if (!handlesTarget)
                continue;
            if (item.name.isEmpty() || item.exec.isEmpty())
                continue;
            if (hiddenSet.contains(id))
                continue;

            item.id = id;
            seenIds.insert(id);
            m_items.append(item);
        }
    }
}

bool BrowserRegistry::parseDesktopEntry(const QString &path, Item &item,
                                        const QStringList &mimeMatches,
                                        bool &handlesTarget)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    bool inEntryGroup = false;
    bool noDisplay = false;
    bool hidden = false;
    QString tryExec;
    QString mimeTypes;

    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        if (line.startsWith(QLatin1Char('['))) {
            inEntryGroup = (line == QLatin1String("[Desktop Entry]"));
            continue;
        }
        if (!inEntryGroup)
            continue;

        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        const QString key = line.left(eq).trimmed();
        const QString value = line.mid(eq + 1).trimmed();

        if (key == QLatin1String("Name")) {
            item.name = value;
        } else if (key == QLatin1String("Icon")) {
            item.iconName = value;
        } else if (key == QLatin1String("Exec")) {
            item.exec = value;
        } else if (key == QLatin1String("MimeType")) {
            mimeTypes = value;
        } else if (key == QLatin1String("NoDisplay")) {
            noDisplay = (value.compare(QLatin1String("true"),
                                       Qt::CaseInsensitive) == 0);
        } else if (key == QLatin1String("Hidden")) {
            hidden = (value.compare(QLatin1String("true"),
                                    Qt::CaseInsensitive) == 0);
        } else if (key == QLatin1String("TryExec")) {
            tryExec = value;
        }
    }

    if (noDisplay || hidden)
        return false;
    if (!tryExec.isEmpty()
        && QStandardPaths::findExecutable(tryExec).isEmpty())
        return false;

    const QStringList mimeTypesFound =
        mimeTypes.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    handlesTarget = false;
    for (const QString &match : mimeMatches) {
        if (mimeTypesFound.contains(match)) {
            handlesTarget = true;
            break;
        }
    }
    return true;
}

} // namespace Luch
