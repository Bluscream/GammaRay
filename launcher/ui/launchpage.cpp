/*
  launchpage.cpp

  This file is part of GammaRay, the Qt application inspection and manipulation tool.

  SPDX-FileCopyrightText: 2011 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: Volker Krause <volker.krause@kdab.com>

  SPDX-License-Identifier: GPL-2.0-or-later

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include <config-gammaray.h>
#include "launchpage.h"
#include "ui_launchpage.h"
#include "probeabimodel.h"

#include <launcher/core/launchoptions.h>
#include <launcher/core/probefinder.h>

#include <QCompleter>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QSettings>
#include <QStringListModel>

using namespace GammaRay;

LaunchPage::LaunchPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::LaunchPage)
    , m_argsModel(new QStringListModel(this))
    , m_abiModel(new ProbeABIModel(this))
    , m_abiIsValid(true)
{
    ui->setupUi(this);
    ui->errorLabel->hide();
#if defined(Q_OS_MAC)
    QMargins margins = ui->formLayout->contentsMargins();
    margins.setRight(margins.right() + 2);
    margins.setBottom(margins.bottom() + 2);
    ui->formLayout->setContentsMargins(margins);
#endif
    connect(ui->progSelectButton, &QAbstractButton::clicked, this, &LaunchPage::showFileDialog);
    connect(ui->workDirSelectButton, &QAbstractButton::clicked, this, &LaunchPage::showDirDialog);
    connect(ui->addArgButton, &QAbstractButton::clicked, this, &LaunchPage::addArgument);
    connect(ui->removeArgButton, &QAbstractButton::clicked, this, &LaunchPage::removeArgument);
    connect(ui->progEdit, &QLineEdit::textChanged, this, &LaunchPage::detectABI);
    connect(ui->progEdit, &QLineEdit::textChanged, this, &LaunchPage::updateButtonState);

    ui->argsBox->setModel(m_argsModel);

    auto *pathCompleter = new QCompleter(this);
    auto *fsModel = new QFileSystemModel(this);
    fsModel->setRootPath(QDir::rootPath());
    pathCompleter->setModel(fsModel);
    ui->progEdit->setCompleter(pathCompleter);

    ui->probeBox->setModel(m_abiModel);

    QSettings settings;
    ui->progEdit->setText(settings.value(QStringLiteral("Launcher/Program")).toString());
    ui->workDirEdit->setText(settings.value(QStringLiteral("Launcher/WorkingDirectory")).toString());
    m_argsModel->setStringList(settings.value(QStringLiteral("Launcher/Arguments")).toStringList());
    ui->accessMode->setCurrentIndex(settings.value(QStringLiteral("Launcher/AccessMode")).toInt());
    updateArgumentButtons();
}

LaunchPage::~LaunchPage()
{
    delete ui;
}

void LaunchPage::writeSettings()
{
    QSettings settings;
    settings.setValue(QStringLiteral("Launcher/Program"), ui->progEdit->text());
    settings.setValue(QStringLiteral("Launcher/WorkingDirectory"), ui->workDirEdit->text());
    settings.setValue(QStringLiteral("Launcher/Arguments"), notEmptyString(m_argsModel->stringList()));
    settings.setValue(QStringLiteral("Launcher/AccessMode"), ui->accessMode->currentIndex());
}

QStringList LaunchPage::notEmptyString(const QStringList &list)
{
    QStringList notEmptyStringList;
    const int numberOfArguments = list.size();
    for (int i = 0; i < numberOfArguments; ++i) {
        if (!list.at(i).trimmed().isEmpty())
            notEmptyStringList << list.at(i);
    }
    return notEmptyStringList;
}

LaunchOptions LaunchPage::launchOptions() const
{
    LaunchOptions opt;

    QStringList l;
    l.push_back(ui->progEdit->text());
    l.append(notEmptyString(m_argsModel->stringList()));
    opt.setLaunchArguments(l);
    opt.setProbeABI(ui->probeBox->itemData(ui->probeBox->currentIndex()).value<ProbeABI>());

    opt.setWorkingDirectory(ui->workDirEdit->text());

    switch (ui->accessMode->currentIndex()) {
    case 0: // local, out-of-process
        opt.setProbeSetting(QStringLiteral("RemoteAccessEnabled"), true);
        opt.setProbeSetting(QStringLiteral("ServerAddress"), GAMMARAY_DEFAULT_LOCAL_TCP_URL);
        opt.setUiMode(LaunchOptions::OutOfProcessUi);
        break;
    case 1: // remote, out-of-process
        opt.setProbeSetting(QStringLiteral("RemoteAccessEnabled"), true);
        opt.setProbeSetting(QStringLiteral("ServerAddress"), GAMMARAY_DEFAULT_ANY_TCP_URL);
        opt.setUiMode(LaunchOptions::OutOfProcessUi);
        break;
    case 2: // in-process
        opt.setProbeSetting(QStringLiteral("RemoteAccessEnabled"), false);
        opt.setUiMode(LaunchOptions::InProcessUi);
        break;
    }

    return opt;
}

void LaunchPage::showFileDialog()
{
    QString exeFilePath = QFileDialog::getOpenFileName(
        this,
        tr("Executable to Launch"),
        ui->progEdit->text()
#ifdef Q_OS_WIN
            ,
        tr("Executable (*.exe)")
#endif
    );

    if (exeFilePath.isEmpty())
        return;

    {
        const QFileInfo fileInfo(exeFilePath);
        if (fileInfo.isBundle()) {
            const QString bundleTarget = QString::fromLatin1("%1/Contents/MacOS/%2")
                                             .arg(exeFilePath, fileInfo.completeBaseName())
                                             .replace(QLatin1String("/"), QDir::separator());
            if (QFile::exists(bundleTarget))
                exeFilePath = bundleTarget;
        }
    }

    ui->progEdit->setText(exeFilePath);
}

void LaunchPage::showDirDialog()
{
    QString workingDirPath = QFileDialog::getExistingDirectory(
        this, tr("Working Directory"), ui->workDirEdit->text());

    if (!workingDirPath.isEmpty()) {
        ui->workDirEdit->setText(workingDirPath);
    }
}

void LaunchPage::addArgument()
{
    m_argsModel->insertRows(m_argsModel->rowCount(), 1);
    const QModelIndex newIndex = m_argsModel->index(m_argsModel->rowCount() - 1, 0);
    ui->argsBox->edit(newIndex);
    updateArgumentButtons();
}

void LaunchPage::removeArgument()
{
    // TODO check if there's a selection at all and update button state accordingly
    m_argsModel->removeRows(ui->argsBox->currentIndex().row(), 1);
    updateArgumentButtons();
}

bool LaunchPage::isValid()
{
    return m_abiIsValid && fileIsExecutable();
}

bool LaunchPage::fileIsExecutable() const
{
    if (ui->progEdit->text().isEmpty())
        return false;

    const QFileInfo fi(ui->progEdit->text());

#ifdef Q_OS_MAC
    if (fi.isBundle() && (fi.suffix() == "app"))
        return true;

#endif

    return fi.exists() && fi.isFile() && fi.isExecutable();
}

void LaunchPage::updateArgumentButtons()
{
    ui->removeArgButton->setEnabled(m_argsModel->rowCount() > 0);
}

void LaunchPage::detectABI(const QString &path)
{
    const ProbeABI abi = m_abiDetector.abiForExecutable(path);
    const int index = m_abiModel->indexOfBestMatchingABI(abi);
    if (index >= 0) {
        ui->probeBox->setCurrentIndex(index);
    } else {
        if (!fileIsExecutable()) {
            ui->errorLabel->setText(tr("The given file path is not a valid executable"));
        } else {
            ui->errorLabel->setText(tr("None of the available ABI probes supports the ABI of this binary [%1]").arg(abi.displayString()));
        }
    }
    m_abiIsValid = index >= 0;
    ui->errorLabel->setVisible(!m_abiIsValid && !ui->progEdit->text().isEmpty());
    emit updateButtonState();
}
