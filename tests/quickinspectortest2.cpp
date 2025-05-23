/*
  quickinspectortest2.cpp

  This file is part of GammaRay, the Qt application inspection and manipulation tool.

  SPDX-FileCopyrightText: 2015 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: Volker Krause <volker.krause@kdab.com>

  SPDX-License-Identifier: GPL-2.0-or-later

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "basequicktest.h"
#include "testhelpers.h"

#include <plugins/quickinspector/quickinspectorinterface.h>
#include <common/objectbroker.h>
#include <common/remoteviewinterface.h>
#include <common/remoteviewframe.h>

#include <QAbstractItemModelTester>
#include <QItemSelectionModel>
#include <QQuickItem>

using namespace GammaRay;
using namespace TestHelpers;

class QuickInspectorTest2 : public BaseQuickTest
{
    Q_OBJECT
protected:
    bool ignoreNonExposedView() const override
    {
        return true;
    }

private slots:
    static void initTestCase()
    {
        qRegisterMetaType<QItemSelection>();
    }

    void init() override
    {
        BaseQuickTest::init();

        itemModel = ObjectBroker::model(QStringLiteral("com.kdab.GammaRay.QuickItemModel"));
        QVERIFY(itemModel);
        new QAbstractItemModelTester(itemModel, view());

        sgModel = ObjectBroker::model(QStringLiteral("com.kdab.GammaRay.QuickSceneGraphModel"));
        QVERIFY(sgModel);
        new QAbstractItemModelTester(sgModel, view());

        inspector = ObjectBroker::object<QuickInspectorInterface *>();
        QVERIFY(inspector);
        inspector->setServerSideDecorationsEnabled(false);
        inspector->selectWindow(0);
        QTest::qWait(1);
    }

    static void testPreviewFetchingThrottler_data()
    {
        QTest::addColumn<bool>("clientIsReplying", nullptr);
        QTest::newRow("no-reply") << false;
        QTest::newRow("reply") << true;
    }

    void testPreviewFetchingThrottler()
    {
        QFETCH(bool, clientIsReplying);

        auto remoteView = ObjectBroker::object<RemoteViewInterface *>(QStringLiteral(
            "com.kdab.GammaRay.QuickRemoteView"));
        QVERIFY(remoteView);

        QVERIFY(showSource(QStringLiteral("qrc:/manual/rotationinvariant.qml")));

        if (!isViewExposed())
            return;

        auto rootItem = view()->rootObject();
        QVERIFY(rootItem);
        Probe::instance()->selectObject(rootItem, QPoint());

        // RemoteViewServer throttle the requests with an interval of qRound(1000.0 / 60.0)
        const qreal throttlerInterval = qRound(1000.0 / 60.0);

        if (clientIsReplying) {
            connect(remoteView, &RemoteViewInterface::frameUpdated,
                    remoteView, &RemoteViewInterface::clientViewUpdated, Qt::UniqueConnection);
        } else {
            disconnect(remoteView, &RemoteViewInterface::frameUpdated,
                       remoteView, &RemoteViewInterface::clientViewUpdated);
        }

        QSignalSpy requestedSpy(remoteView, SIGNAL(requestUpdate()));
        QVERIFY(requestedSpy.isValid());

        QSignalSpy updatedSpy(remoteView, &RemoteViewInterface::frameUpdated);
        QVERIFY(updatedSpy.isValid());

        // Testing static scene only send 1 frame
        for (int i = 0; i < 3; ++i) {
            remoteView->setViewActive(true);
            // Activating the view trigger an update request
            QVERIFY(waitForSignal(&updatedSpy, true));
            QVERIFY(requestedSpy.size() == 1 || requestedSpy.size() == 2); // should be 1, but we might see spurious repaints on windows
            QVERIFY(updatedSpy.size() == 1 || updatedSpy.size() == 2);
            if (!clientIsReplying)
                remoteView->clientViewUpdated();

            requestedSpy.clear();
            updatedSpy.clear();

            if (clientIsReplying) {
                // The client is answering clientViewUpdated automatically.
                // For 1 request and multiple client answers the server should only send 1 frame.

                view()->update();
                QVERIFY(waitForSignal(&updatedSpy, true));

                for (int j = 0; j < 3; ++j) {
                    // The automatic clientViewUpdated answer is only done for frame sent.
                    // Let manually trigger answers.
                    QTest::qWait(throttlerInterval);
                    remoteView->clientViewUpdated();
                }

                QVERIFY(waitForSignal(&requestedSpy, true));
                QVERIFY(requestedSpy.size() == 1 || requestedSpy.size() == 2);
                QVERIFY(updatedSpy.size() == 1 || updatedSpy.size() == 2);
            } else {
                // The client is not answering with clientViewUpdated automatically.
                // Only 1 request and 1 frame sent should trigger.

                for (int i = 0; i < 3; ++i) {
                    view()->update();
                    QTest::qWait(throttlerInterval);
                    QVERIFY(waitForSignal(&updatedSpy, true));
                }

                QVERIFY(requestedSpy.size() == 1 || requestedSpy.size() == 2);
                QVERIFY(updatedSpy.size() == 1 || updatedSpy.size() == 2);
            }

            requestedSpy.clear();
            updatedSpy.clear();

            remoteView->setViewActive(false);
        }

        // Our animation properties
        const qreal animationInterval = throttlerInterval;
        const qreal animationDuration = 100.0;
        // Qml try to render @ ~60fps
        const qreal maxPossibleQmlRequests =
            clientIsReplying ? animationDuration / 1000.0 * 60.0 : 1.0;
        const qreal maxPossibleThrottledRequests =
            clientIsReplying ? qMin(maxPossibleQmlRequests, animationDuration / throttlerInterval) : 1.0;

        // Testing dynamic scene
        for (int i = 0; i < 3; i++) {
            remoteView->setViewActive(true);
            // Activating the view trigger an update request
            QVERIFY(waitForSignal(&updatedSpy, true));
            QCOMPARE(requestedSpy.size(), 1);
            QCOMPARE(updatedSpy.size(), 1);
            if (!clientIsReplying)
                remoteView->clientViewUpdated();

            requestedSpy.clear();
            updatedSpy.clear();

            // We rotate our rendering every animationInterval-ms for animationDuration-ms
            rootItem->setProperty("interval", qRound(animationInterval));
            rootItem->setProperty("animated", true);
            QTest::qWait(qRound(animationDuration));

            // Wait to process pending requests...
            rootItem->setProperty("animated", false);
            QTest::qWait(qRound(animationDuration));

            QVERIFY(requestedSpy.size() <= qRound(maxPossibleThrottledRequests * 1.05) + 1);
            QVERIFY(updatedSpy.size() <= qRound(maxPossibleThrottledRequests * 1.05) + 1);
            QCOMPARE(requestedSpy.size(), updatedSpy.size());

            requestedSpy.clear();
            updatedSpy.clear();

            remoteView->setViewActive(false);
        }
    }

private:
    QAbstractItemModel *itemModel;
    QAbstractItemModel *sgModel;
    QuickInspectorInterface *inspector;
};

QTEST_MAIN(QuickInspectorTest2)

#include "quickinspectortest2.moc"
