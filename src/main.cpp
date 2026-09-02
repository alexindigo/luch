#include <LayerShellQt/Shell>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QUrl>

#include <cstring>

#include "augmentationpipeline.h"
#include "browserregistry.h"
#include "config.h"
#include "dbustransport.h"
#include "launcher.h"
#include "pickerwindow.h"
#include "queue.h"
#include "singleinstance.h"
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

    Luch::AugmentationPipeline pipeline;
    pipeline.discoverAndLoad();
    const QVariantMap payload = pipeline.run(target.toMap());

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
    Luch::AugmentationPipeline pipeline;
    pipeline.discoverAndLoad();

    target.pluginData = pipeline.run(target.toMap());
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
                     [&queue, &pipeline](const QString &raw) {
                         Luch::Target t;
                         QString err;
                         if (Luch::Target::parse(raw, t, &err)) {
                             t.pluginData = pipeline.run(t.toMap());
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
