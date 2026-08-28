#pragma once

#include <QString>

namespace Luch {

struct Target {
    enum Kind {
        Url,
        HtmlFile,
    };

    Kind kind = Url;
    QString raw;
    QString path;
    QString urlForm;
    QString scheme;
    QString hostOrDir;
    QString middle;
    QString tail;

    static bool parse(const QString &argument, Target &target,
                      QString *errorMessage);
};

} // namespace Luch
