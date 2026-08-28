#include <LayerShellQt/Shell>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QUrl>

#include <cstring>

#include "browserregistry.h"
#include "config.h"
#include "launcher.h"
#include "pickerwindow.h"
#include "target.h"
#include "xdgactivation.h"

namespace {

int setDefaultHandler()
{
    const QString xdgMime =
        QStandardPaths::findExecutable(QStringLiteral("xdg-mime"));
    if (xdgMime.isEmpty()) {
        qCritical().noquote() << QStringLiteral("luch: xdg-mime not found");
        return 1;
    }

    const QStringList schemes = {QStringLiteral("x-scheme-handler/http"),
                                 QStringLiteral("x-scheme-handler/https")};
    QStringList args = {QStringLiteral("default"),
                        QStringLiteral("luch.desktop")};
    args += schemes;
    const int rc = QProcess::execute(xdgMime, args);
    if (rc != 0) {
        qCritical().noquote()
            << QStringLiteral("luch: xdg-mime default failed (%1)").arg(rc);
        return rc;
    }

    for (const QString &scheme : schemes) {
        QProcess query;
        query.start(xdgMime, {QStringLiteral("query"),
                              QStringLiteral("default"), scheme});
        query.waitForFinished();
        printf("  %s -> %s", qPrintable(scheme),
               qPrintable(QString::fromUtf8(query.readAllStandardOutput()
                                                .trimmed())));
        printf("\n");
    }
    printf("luch is now the default handler for http and https.\n");
    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "--set-default") == 0)
            return setDefaultHandler();
    }

#if defined(__GNUC__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    LayerShellQt::Shell::useLayerShell();
#if defined(__GNUC__)
#    pragma GCC diagnostic pop
#endif

    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Luch"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QCoreApplication::setOrganizationName(QStringLiteral("luch"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Luch — link router: pick which browser opens a URL "
                       "or local HTML file."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption setDefaultOption(
        QStringLiteral("set-default"),
        QStringLiteral("Register luch as the default http/https handler "
                       "via xdg-mime, then exit."));
    parser.addOption(setDefaultOption);
    parser.addPositionalArgument(
        QStringLiteral("target"),
        QStringLiteral("URL or local HTML file to route to a browser."));
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.isEmpty()) {
        parser.showHelp(2);
    }

    Luch::Target target;
    QString parseError;
    if (!Luch::Target::parse(args.first(), target, &parseError)) {
        qCritical().noquote() << parseError;
        return 2;
    }

    const QStringList mimeMatches =
        target.kind == Luch::Target::HtmlFile
            ? QStringList{QStringLiteral("text/html"),
                          QStringLiteral("application/xhtml+xml")}
            : QStringList{QStringLiteral("x-scheme-handler/http")};

    Luch::Config config;
    Luch::BrowserRegistry registry(&config, mimeMatches);
    Luch::Launcher launcher;
    launcher.setTarget(target);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("incomingUrl"),
                                             target.raw);
    engine.rootContext()->setContextProperty(QStringLiteral("targetScheme"),
                                             target.scheme);
    engine.rootContext()->setContextProperty(QStringLiteral("targetHostOrDir"),
                                             target.hostOrDir);
    engine.rootContext()->setContextProperty(QStringLiteral("targetMiddle"),
                                             target.middle);
    engine.rootContext()->setContextProperty(QStringLiteral("targetTail"),
                                             target.tail);
    engine.rootContext()->setContextProperty(QStringLiteral("browserRegistry"),
                                             &registry);
    engine.rootContext()->setContextProperty(QStringLiteral("launcher"),
                                             &launcher);
    engine.load(QUrl(QStringLiteral("qrc:/Luch/ui/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        qCritical() << "luch: QML root failed to load";
        return 1;
    }

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window) {
        qCritical() << "luch: QML root is not a Window";
        return 1;
    }

    XdgActivationTokenRequester activationRequester;
    if (config.focusFollowOnOpen())
        launcher.setActivationSource(&activationRequester, window);

    bool done = false;
    Luch::PickerWindow picker(window);
    QObject::connect(&picker, &Luch::PickerWindow::dismissed, &app,
                     [&app, &done](int exitCode) {
                         if (done)
                             return;
                         done = true;
                         app.exit(exitCode);
                     });
    QObject::connect(&launcher, &Luch::Launcher::launched, &app,
                     [&app, &done] {
                         if (done)
                             return;
                         done = true;
                         app.exit(0);
                     });
    QObject::connect(&launcher, &Luch::Launcher::copied, &app,
                     [&app, &done] {
                         if (done)
                             return;
                         done = true;
                         app.exit(0);
                     });
    window->show();

    return app.exec();
}
