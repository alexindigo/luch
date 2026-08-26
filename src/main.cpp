#include <LayerShellQt/Shell>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QUrl>

#include "browserregistry.h"
#include "config.h"
#include "launcher.h"
#include "pickerwindow.h"

int main(int argc, char *argv[])
{
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
        QStringLiteral("Luch — link router: pick which browser opens a URL."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("url"),
                                 QStringLiteral("URL to route to a browser."));
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.isEmpty()) {
        parser.showHelp(2);
    }

    const QUrl url(args.first());
    if (!url.isValid()
        || (url.scheme() != QLatin1String("http")
            && url.scheme() != QLatin1String("https"))) {
        qCritical().noquote()
            << QStringLiteral("luch: not a routable http(s) URL:")
            << args.first();
        return 2;
    }

    Luch::Config config;
    Luch::BrowserRegistry registry(&config);
    Luch::Launcher launcher;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("incomingUrl"),
                                             url.toString());
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
    window->show();

    return app.exec();
}
