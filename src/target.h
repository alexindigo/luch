#pragma once

#include <QString>
#include <QVariantMap>

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

    // Full augmentation payload — {"original": targetMap,
    // "url": <effective>, "detected": […], "trace": […]} — filled by
    // the augmentation pipeline at append time. The target itself stays
    // original-canonical — plugins only add data.
    QVariantMap pluginData;

    // The plugin-facing flat view: {"kind": "url"|"htmlfile",
    // "url": urlForm, "raw": raw arg}.
    QVariantMap toMap() const;

    static bool parse(const QString &argument, Target &target,
                      QString *errorMessage);
};

} // namespace Luch
