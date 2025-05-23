/*
  launcher.cpp

  This file is part of GammaRay, the Qt application inspection and manipulation tool.

  SPDX-FileCopyrightText: 2013 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: Volker Krause <volker.krause@kdab.com>

  SPDX-License-Identifier: GPL-2.0-or-later

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "launcher.h"
#include "probefinder.h"
#include "probeabi.h"
#include "clientlauncher.h"
#include "launchoptions.h"
#include "injector/abstractinjector.h"
#include "injector/injectorfactory.h"

#include <common/endpoint.h>
#include <common/message.h>
#include <common/paths.h>

#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QNetworkInterface>
#include <QUrl>

#include <iostream>

namespace GammaRay {
enum State
{
    Initial = 0,
    InjectorFinished = 1,
    InjectorFailed = 2,
    ClientStarted = 4,
    Complete = InjectorFinished | ClientStarted
};

struct LauncherPrivate
{
    explicit LauncherPrivate(const LaunchOptions &options)
        : options(options)
        , server(nullptr)
        , socket(nullptr)
        , state(Initial)
        , exitCode(0)
    {
    }

    AbstractInjector::Ptr createInjector(QStringList *errorStrings = nullptr) const
    {
        if (options.injectorType().isEmpty()) {
            if (options.isAttach()) {
                return InjectorFactory::defaultInjectorForAttach(errorStrings);
            } else {
                return InjectorFactory::defaultInjectorForLaunch(options.probeABI(), errorStrings);
            }
        }
        return InjectorFactory::createInjector(
            options.injectorType(), options.injectorTypeExecutableOverride());
    }

    LaunchOptions options;
    QLocalServer *server;
    QLocalSocket *socket;
    ClientLauncher client;
    QTimer safetyTimer;
    AbstractInjector::Ptr injector;
    QUrl serverAddress;
    QString errorMessage;
    int state;
    int exitCode;
};
}

using namespace GammaRay;

Launcher::Launcher(const LaunchOptions &options, QObject *parent)
    : QObject(parent)
    , d(new LauncherPrivate(options))
{
    Q_ASSERT(options.isValid());

    const auto timeout = qEnvironmentVariableIntValue("GAMMARAY_LAUNCHER_TIMEOUT");
    d->safetyTimer.setInterval(std::max(60, timeout) * 1000);
    d->safetyTimer.setSingleShot(true);
    connect(&d->safetyTimer, &QTimer::timeout, this, &Launcher::timeout);
}

Launcher::~Launcher()
{
    stop();
    d->client.waitForFinished();
    delete d;
}

qint64 Launcher::instanceIdentifier() const
{
    if (d->options.isAttach())
        return d->options.pid();

    return QCoreApplication::applicationPid();
}

void Launcher::stop()
{
    if (d->injector)
        d->injector->stop();
}

static QProcessEnvironment addTargetPluginPaths(QProcessEnvironment env, const ProbeABI &abi)
{
    QString qtPluginPath = env.value(QStringLiteral("QT_PLUGIN_PATH"));
    if (!qtPluginPath.isEmpty()) {
        qtPluginPath.append(QDir::listSeparator());
    }
    qtPluginPath.append(Paths::targetPluginPaths(abi.id()).join(QDir::listSeparator()));
    env.insert(QStringLiteral("QT_PLUGIN_PATH"), qtPluginPath);
    return env;
}

bool Launcher::start()
{
    auto probeDll = d->options.probePath();
    if (probeDll.isEmpty()) {
        probeDll = ProbeFinder::findProbe(d->options.probeABI());
        d->options.setProbePath(QFileInfo(probeDll).absolutePath());
    }

    sendLauncherId();
    setupProbeSettingsServer();

    if (d->options.uiMode() != LaunchOptions::InProcessUi)
        d->safetyTimer.start();

    QStringList errorStrings;
    d->injector = d->createInjector(&errorStrings);
    if (!d->injector) {
        Q_ASSERT(!errorStrings.isEmpty());
        std::cerr << "Potential errors:" << std::endl;
        for (const QString &errorString : std::as_const(errorStrings)) {
            injectorError(-1, errorString);
        }
        std::cerr << std::endl;

        if (d->options.injectorType().isEmpty()) {
            if (d->options.isAttach()) {
                injectorError(-1,
                              tr("Uh-oh, there is no default attach injector on this platform."));
            } else {
                injectorError(-1,
                              tr("Uh-oh, there is no default launch injector on this platform."));
            }
        } else {
            injectorError(-1, tr("Injector %1 not found.").arg(d->options.injectorType()));
        }
        std::cerr << "See <https://github.com/KDAB/GammaRay/wiki/Known-Issues> for troubleshooting"
                  << std::endl;
        return false;
    }

    d->injector->setTargetAbi(d->options.probeABI());

    connect(d->injector.data(), &AbstractInjector::started,
            this, &Launcher::restartTimer);
    connect(d->injector.data(), &AbstractInjector::finished,
            this, &Launcher::injectorFinished, Qt::QueuedConnection);
    if (d->options.isLaunch()) {
        connect(d->injector.data(), &AbstractInjector::attached,
                this, &Launcher::injectorFinished, Qt::QueuedConnection);
    }
    connect(d->injector.data(), &AbstractInjector::stderrMessage,
            this, &Launcher::stderrMessage);
    connect(d->injector.data(), &AbstractInjector::stdoutMessage,
            this, &Launcher::stdoutMessage);

    bool success = false;
    if (d->options.isLaunch()) {
        d->injector->setWorkingDirectory(d->options.workingDirectory());
        success = d->injector->launch(d->options.launchArguments(), probeDll,
                                      QStringLiteral("gammaray_probe_inject"),
                                      addTargetPluginPaths(d->options.processEnvironment(), d->options.probeABI()));
    } else if (d->options.isAttach()) {
        success = d->injector->attach(d->options.pid(), probeDll,
                                      QStringLiteral("gammaray_probe_attach"));
    }

    if (!success) {
        QString errorMessage;
        if (d->options.isLaunch()) {
            errorMessage = tr("Failed to launch target '%1'.").arg(d->options.launchArguments().join(QStringLiteral(" ")));
        }
        if (d->options.isAttach()) {
            errorMessage = tr("Failed to attach to target with PID %1.").arg(d->options.pid());
        }
        if (!d->injector->errorString().isEmpty()) {
            errorMessage += tr("\nError: %1").arg(d->injector->errorString());
        }
        injectorError(d->injector->exitCode() ? d->injector->exitCode() : 1, errorMessage);
        std::cerr << "See <https://github.com/KDAB/GammaRay/wiki/Known-Issues> for troubleshooting"
                  << std::endl;
        return false;
    }
    return true;
}

int Launcher::exitCode() const
{
    return d->exitCode;
}

QString Launcher::errorMessage() const
{
    return d->errorMessage;
}

QUrl Launcher::serverAddress() const
{
    return d->serverAddress;
}

void Launcher::sendLauncherId()
{
    // if we are launching a new process, make sure it knows how to talk to us
    if (d->options.isLaunch())
        d->options.setProbeSetting(QStringLiteral("LAUNCHER_ID"), instanceIdentifier());
}

void Launcher::setupProbeSettingsServer()
{
    d->server = new QLocalServer(this);
    d->server->setMaxPendingConnections(1);
    connect(d->server, &QLocalServer::newConnection, this, &Launcher::newConnection);
    const QString serverName = QStringLiteral("gammaray-") + QString::number(instanceIdentifier());
    d->server->removeServer(serverName);
    if (!d->server->listen(serverName))
        qWarning() << "Unable to send probe settings:" << d->server->errorString();
}

void Launcher::printAllAvailableIPs()
{
    std::cout << "GammaRay server listening on:"
              << std::endl;

    foreach (const QNetworkInterface &inter, QNetworkInterface::allInterfaces()) {
        if (!(inter.flags() & QNetworkInterface::IsUp) || !(inter.flags() & QNetworkInterface::IsRunning) || (inter.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        foreach (const QNetworkAddressEntry &addrEntry, inter.addressEntries()) {
            const QHostAddress addr = addrEntry.ip();
            std::cout << "  interface " << qPrintable(inter.name()) << ": "
                      << qPrintable(addr.toString()) << ":"
                      << d->serverAddress.port()
                      << std::endl;
        }
    }
}

void Launcher::startClient(const QUrl &serverAddress)
{
    if (!d->client.launch(serverAddress)) {
        qCritical("Unable to launch gammaray-client!");
        QCoreApplication::exit(1);
    }
}

void Launcher::injectorFinished()
{
    d->exitCode = d->injector->exitCode();
    if (d->errorMessage.isEmpty()) {
        d->errorMessage = d->injector->errorString();
        if (!d->errorMessage.isEmpty()) {
            d->state |= InjectorFailed;
            std::cerr << "Injector error: " << qPrintable(d->errorMessage) << std::endl;
        }
    }

    if ((d->state & InjectorFailed) == 0) {
        d->state |= InjectorFinished;
    }

    checkDone();
}

void Launcher::injectorError(int exitCode, const QString &errorMessage)
{
    d->exitCode = exitCode;
    d->errorMessage += errorMessage + "\n\n";

    d->state |= InjectorFailed;
    std::cerr << qPrintable(errorMessage) << std::endl;

    checkDone();
}

void Launcher::timeout()
{
    d->state |= InjectorFailed;

    std::cerr << "Target not responding - timeout. Try setting the env variable GAMMARAY_LAUNCHER_TIMEOUT to a bigger value (in seconds)." << std::endl;
    std::cerr << "See <https://github.com/KDAB/GammaRay/wiki/Known-Issues> for troubleshooting"
              << std::endl;
    checkDone();
}

void Launcher::restartTimer()
{
    d->safetyTimer.stop();
    d->safetyTimer.start();
}

void Launcher::checkDone()
{
    if (d->state == Complete || d->state == InjectorFinished) {
        emit finished();
    } else if ((d->state & InjectorFailed) != 0) {
        d->client.terminate();
        if (d->exitCode == 0) {
            d->exitCode = 1;
        }
        emit finished();
    }
}

void Launcher::newConnection()
{
    if (d->socket)
        return;

    d->socket = d->server->nextPendingConnection();
    connect(d->socket, &QIODevice::readyRead, this, &Launcher::readyRead);

    {
        Message msg(Protocol::LauncherAddress, Protocol::ServerVersion);
        msg << Protocol::version();
        msg.write(d->socket);
    }

    {
        Message msg(Protocol::LauncherAddress, Protocol::ProbeSettings);
        msg << d->options.probeSettings();
        msg.write(d->socket);
    }
}

void Launcher::readyRead()
{
    while (Message::canReadMessage(d->socket)) {
        const auto msg = Message::readMessage(d->socket);
        switch (msg.type()) {
        case Protocol::ServerAddress: {
            msg >> d->serverAddress;
            break;
        }
        case Protocol::ServerLaunchError: {
            QString reason;
            msg >> reason;
            std::cerr << "Failed to start server: " << qPrintable(reason)
                      << std::endl;
            // TODO emit error signal to also notify qtcreator, etc
            break;
        }
        default:
            continue;
        }
    }

    if (d->serverAddress.isEmpty())
        return;

    d->safetyTimer.stop();
    const auto listenAddress = d->options.probeSettings().value("ServerAddress");
    if (listenAddress.isEmpty() || listenAddress.startsWith("tcp://0.0.0.0")) {
        printAllAvailableIPs();
    } else {
        std::cout << "GammaRay server listening on: " << qPrintable(d->serverAddress.toString())
                  << std::endl;
    }

    if (d->options.uiMode() == LaunchOptions::OutOfProcessUi)
        startClient(d->serverAddress);

    if (d->options.isAttach())
        emit attached();

    d->state |= ClientStarted;
    checkDone();
    emit started();
}
