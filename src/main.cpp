#include <LayerShellQt/Shell>
#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QUrl>

#include <cstring>

#include "augmentationpipeline.h"
#include "browserregistry.h"
#include "config.h"
#include "dbustransport.h"
#include "launcher.h"
#include "pickerwindow.h"
#include "queue.h"
#include "settings.h"
#include "singleinstance.h"
#include "target.h"
#include "urltools.h"
#include "xdgactivation.h"

namespace {

void logPayload(const QVariantMap &payload)
{
    qInfo().noquote()
        << "luch payload:"
        << QString::fromUtf8(
               QJsonDocument(QJsonObject::fromVariantMap(payload))
                   .toJson(QJsonDocument::Compact));
}

// `luch --daemon`: the minimal resident settings daemon — its whole
// job is settings + tray; pickers stay short-lived and read settings
// at startup.
int runDaemon(int argc, char *argv[])
{
    // QApplication (not QGuiApplication): QSystemTrayIcon + QMenu are
    // QWidget-based.
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Luch"));
    QCoreApplication::setOrganizationName(QStringLiteral("luch"));

    Luch::Settings settings;
    settings.save(); // daemon-owned: materialize (incl. one-time migration)

    QSystemTrayIcon tray(QIcon::fromTheme(QStringLiteral("luch")));
    tray.setToolTip(QStringLiteral("luch — link router settings"));

    QMenu menu;
    QAction *const dissectionAction =
        menu.addAction(QObject::tr("Always show bottom panel"));
    dissectionAction->setCheckable(true);
    dissectionAction->setChecked(settings.showDissection());
    QObject::connect(dissectionAction, &QAction::toggled, &settings,
                     &Luch::Settings::setShowDissection);

    QAction *const logAction = menu.addAction(QObject::tr("Log payload"));
    logAction->setCheckable(true);
    logAction->setChecked(settings.logPayload());
    QObject::connect(logAction, &QAction::toggled, &settings,
                     &Luch::Settings::setLogPayload);

    menu.addSeparator();
    QAction *const quitAction = menu.addAction(QObject::tr("Quit"));
    QObject::connect(quitAction, &QAction::triggered, &app,
                     &QCoreApplication::quit);

    tray.setContextMenu(&menu);
    tray.show();
    return app.exec();
}

// `luch --settings`: the settings UI directly — the escape hatch for
// watcher-less environments (stock GNOME, test VMs).
int runSettings(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Luch"));
    QCoreApplication::setOrganizationName(QStringLiteral("luch"));

    Luch::Settings settings;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("settings"),
                                             &settings);
    engine.load(QUrl(QStringLiteral("qrc:/Luch/ui/SettingsWindow.qml")));
    if (engine.rootObjects().isEmpty()) {
        qCritical() << "luch: settings UI failed to load";
        return 1;
    }
    return app.exec();
}

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

// `luch --inspect <url>`: QtCore-only early-out (like --set-default) —
// parse the target, run the augmentation pipeline, print
// {"roster":[…],"payload":{…}} as indented JSON. No Wayland, no QML.
int inspectTarget(int argc, char *argv[], const QString &argument)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Luch"));
    QCoreApplication::setOrganizationName(QStringLiteral("luch"));

    Luch::Target target;
    QString parseError;
    if (!Luch::Target::parse(argument, target, &parseError)) {
        qCritical().noquote() << parseError;
        return 2;
    }

    Luch::Settings settings;
    Luch::AugmentationPipeline pipeline;
    pipeline.setPluginConfigs(settings.pluginConfigs());
    pipeline.setMaxHops(settings.maxHops());
    pipeline.discoverAndLoad();
    const QVariantMap payload = pipeline.run(target.toMap());
    if (settings.logPayload())
        logPayload(payload);

    const QJsonObject root{
        {QStringLiteral("roster"),
         QJsonArray::fromVariantList(pipeline.roster())},
        {QStringLiteral("payload"), QJsonObject::fromVariantMap(payload)},
    };
    printf("%s\n",
           QJsonDocument(root).toJson(QJsonDocument::Indented).constData());
    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "--set-default") == 0)
            return setDefaultHandler();
        if (qstrcmp(argv[i], "--daemon") == 0)
            return runDaemon(argc, argv);
        if (qstrcmp(argv[i], "--settings") == 0)
            return runSettings(argc, argv);
        if (qstrcmp(argv[i], "--inspect") == 0) {
            if (i + 1 >= argc) {
                qCritical().noquote()
                    << QStringLiteral("luch: --inspect needs a target");
                return 2;
            }
            return inspectTarget(argc, argv,
                                 QString::fromLocal8Bit(argv[i + 1]));
        }
    }

// Capture the pre-LayerShellQt state of QT_WAYLAND_SHELL_INTEGRATION:
// useLayerShell() below exports it into our environment, and launched
// apps must see the original value (or none), not the layer-shell one.
const bool hadShellIntegration =
    qEnvironmentVariableIsSet("QT_WAYLAND_SHELL_INTEGRATION");
const QString priorShellIntegration =
    qEnvironmentVariable("QT_WAYLAND_SHELL_INTEGRATION");

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

    Luch::SingleInstance single;
    if (!single.tryClaim(target.raw))
        return 0; // handed off to the running instance

    Luch::Config config;
    // Pickers read the unified settings at startup (daemon-owned;
    // they never write).
    Luch::Settings settings;
    Luch::AugmentationPipeline pipeline;
    pipeline.setPluginConfigs(settings.pluginConfigs());
    pipeline.setMaxHops(settings.maxHops());
    pipeline.discoverAndLoad();

    target.pluginData = pipeline.run(target.toMap());
    if (settings.logPayload())
        logPayload(target.pluginData);
    Luch::TargetQueue queue;
    queue.append(target);

    const QStringList mimeMatches =
        target.kind == Luch::Target::HtmlFile
            ? QStringList{QStringLiteral("text/html"),
                          QStringLiteral("application/xhtml+xml")}
            : QStringList{QStringLiteral("x-scheme-handler/http")};

    Luch::BrowserRegistry registry(&config, mimeMatches);
    Luch::Launcher launcher;
    launcher.setTarget(target);
    launcher.setShellIntegrationRestore(hadShellIntegration,
                                        priorShellIntegration);

    // Forwarded targets enter the queue; refused ones are logged and
    // dropped — they must not kill the owner. Forwarded targets run the
    // augmentation pipeline too, before they join the queue.
    QObject::connect(&single, &Luch::SingleInstance::targetArrived, &app,
                     [&queue, &pipeline, &settings](const QString &raw) {
                         Luch::Target t;
                         QString err;
                         if (Luch::Target::parse(raw, t, &err)) {
                             t.pluginData = pipeline.run(t.toMap());
                             if (settings.logPayload())
                                 logPayload(t.pluginData);
                             queue.append(t);
                         } else {
                             qWarning().noquote() << err;
                         }
                     });
    // Late slices from inet plugins land as trace entries on the
    // current target; results for gone targets are dropped by the
    // queue (patchSlice no-ops without a current item).
    QObject::connect(&pipeline, &Luch::AugmentationPipeline::sliceLanded,
                     &queue, &Luch::TargetQueue::patchSlice);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("queue"),
                                             &queue);
    engine.rootContext()->setContextProperty(QStringLiteral("browserRegistry"),
                                             &registry);
    engine.rootContext()->setContextProperty(QStringLiteral("launcher"),
                                             &launcher);
    engine.rootContext()->setContextProperty(QStringLiteral("pluginRoster"),
                                             pipeline.roster());
    engine.rootContext()->setContextProperty(QStringLiteral("pipeline"),
                                             &pipeline);
    Luch::UrlTools urlTools;
    engine.rootContext()->setContextProperty(QStringLiteral("urlTools"),
                                             &urlTools);
    engine.rootContext()->setContextProperty(QStringLiteral("settings"),
                                             &settings);
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
    Luch::DbusTransport dbusTransport;
    if (config.focusFollowOnOpen())
        launcher.setActivationSource(&activationRequester, window);
    launcher.setDbusTransport(&dbusTransport);

    // Queue cursor → per-item target/launcher/registry repopulation.
    QObject::connect(&queue, &Luch::TargetQueue::currentChanged, &app,
                     [&queue, &launcher, &registry] {
                         const Luch::Target *t = queue.current();
                         if (!t)
                             return;
                         launcher.setTarget(*t);
                         registry.setMimeMatches(
                             t->kind == Luch::Target::HtmlFile
                                 ? QStringList{QStringLiteral("text/html"),
                                               QStringLiteral(
                                                   "application/xhtml+xml")}
                                 : QStringList{QStringLiteral(
                                     "x-scheme-handler/http")});
                     });

    bool done = false;
    bool handledAny = false;
    Luch::PickerWindow picker(window);
    QObject::connect(&picker, &Luch::PickerWindow::dismissed, &app,
                     [&app, &done, &handledAny](int) {
                         if (done)
                             return;
                         done = true;
                         app.exit(handledAny ? 0 : 1);
                     });
    QObject::connect(&launcher, &Luch::Launcher::launched, &app,
                     [&queue, &handledAny] {
                         handledAny = true;
                         queue.removeCurrent();
                     });
    QObject::connect(&launcher, &Luch::Launcher::copied, &app,
                     [&queue, &handledAny] {
                         handledAny = true;
                         queue.removeCurrent();
                     });
    QObject::connect(&queue, &Luch::TargetQueue::countChanged, &app,
                     [&app, &done, &handledAny, &queue] {
                         if (done)
                             return;
                         if (queue.count() > 0)
                             return;
                         done = true;
                         app.exit(handledAny ? 0 : 1);
                     });
    window->show();

    return app.exec();
}
