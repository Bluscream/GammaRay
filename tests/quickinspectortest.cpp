/*
  quickinspectortest.cpp

  This file is part of GammaRay, the Qt application inspection and manipulation tool.

  SPDX-FileCopyrightText: 2015 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: Volker Krause <volker.krause@kdab.com>

  SPDX-License-Identifier: GPL-2.0-or-later

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "basequicktest.h"
#include "testhelpers.h"

#include <plugins/quickinspector/quickinspectorinterface.h>
#include <core/problemcollector.h>
#include <common/objectbroker.h>
#include <common/remoteviewinterface.h>
#include <common/remoteviewframe.h>
#include <core/propertydata.h>
#include <core/propertyfilter.h>
#include <core/toolmanager.h>

#include <QAbstractItemModelTester>
#include <QItemSelectionModel>
#include <QSortFilterProxyModel>
#include <QRegularExpression>

#include <QQuickItem>
#include <private/qquickitem_p.h>

using namespace GammaRay;
using namespace TestHelpers;

class QuickInspectorTest : public BaseQuickTest
{
    Q_OBJECT
protected:
    bool ignoreNonExposedView() const override
    {
        return true;
    }

private:
    void triggerSceneChange()
    {
        QTest::keyClick(view(), Qt::Key_Right);
        QTest::qWait(20);
        QTest::keyClick(view(), Qt::Key_Left);
        QTest::qWait(20);
        QTest::keyClick(view(), Qt::Key_Right);
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

    void testModelsReparent()
    {
        QVERIFY(showSource(QStringLiteral("qrc:/manual/reparenttest.qml")));

        QTest::keyClick(view(), Qt::Key_Right);
        QTest::qWait(20);
        QTest::keyClick(view(), Qt::Key_Left);
        QTest::qWait(20);
        QTest::keyClick(view(), Qt::Key_Right);
        QTest::qWait(20);
    }

    void testModelsCreateDestroy()
    {
        QVERIFY(showSource(QStringLiteral("qrc:/manual/quickitemcreatedestroytest.qml")));

        // scroll through the list, to trigger creations/destructions
        for (int i = 0; i < 30; ++i)
            QTest::keyClick(view(), Qt::Key_Down);
        QTest::qWait(20);
        for (int i = 0; i < 30; ++i)
            QTest::keyClick(view(), Qt::Key_Up);
        QTest::qWait(20);
    }

    void testModelsCreateDestroyProxy()
    {
        QVERIFY(showSource(QStringLiteral("qrc:/manual/quickitemcreatedestroytest.qml")));
        QVERIFY(itemModel->rowCount() > 0);
        QVERIFY(sgModel->rowCount() > 0);

        itemModel->setProperty("filterKeyColumn", -1);
        itemModel->setProperty("filterRegExp",
                               QRegularExpression("Rect", QRegularExpression::CaseInsensitiveOption));

        sgModel->setProperty("filterKeyColumn", -1);
        sgModel->setProperty("filterRegExp",
                             QRegularExpression("Transform", QRegularExpression::CaseInsensitiveOption));
        QVERIFY(itemModel->rowCount() > 0);
        QVERIFY(sgModel->rowCount() > 0);

        // scroll through the list, to trigger creations/destructions
        for (int i = 0; i < 30; ++i)
            QTest::keyClick(view(), Qt::Key_Down);
        QTest::qWait(20);

        itemModel->setProperty("filterRegExp", QRegularExpression());
        sgModel->setProperty("filterRegExp", QRegularExpression());

        QTest::qWait(20);
    }

    void testItemPicking()
    {
        QVERIFY(showSource(QStringLiteral("qrc:/manual/reparenttest.qml")));

        ToolManagerInterface *toolManager = ObjectBroker::object<ToolManagerInterface *>();
        QSignalSpy toolSpy(toolManager, SIGNAL(toolSelected(QString)));
        QVERIFY(toolSpy.isValid());

        auto itemSelectionModel = ObjectBroker::selectionModel(itemModel);
        QVERIFY(itemSelectionModel);
        QSignalSpy itemSpy(itemSelectionModel, &QItemSelectionModel::selectionChanged);
        QVERIFY(itemSpy.isValid());

        auto sgSelectionModel = ObjectBroker::selectionModel(sgModel);
        QVERIFY(sgModel);
        QSignalSpy sgSpy(sgSelectionModel, &QItemSelectionModel::selectionChanged);
        QVERIFY(sgSpy.isValid());

        // auto center-click is broken before https://codereview.qt-project.org/141085/
        QTest::mouseClick(view(),
                          Qt::LeftButton,
                          Qt::ShiftModifier | Qt::ControlModifier,
                          QPoint(view()->width() / 2, view()->height() / 2));
        QTest::qWait(20);

        QCOMPARE(toolSpy.size(), 1);
        QCOMPARE(itemSpy.size(), 1);
        if (!isViewExposed())
            return;

        QCOMPARE(sgSpy.size(), 1);
    }

    void testFetchingPreview()
    {
        auto remoteView =
            ObjectBroker::object<RemoteViewInterface *>(
                QStringLiteral("com.kdab.GammaRay.QuickRemoteView"));

        QVERIFY(remoteView);
        remoteView->setViewActive(true);

        QSignalSpy renderSpy(view(), &QQuickWindow::frameSwapped);
        QVERIFY(renderSpy.isValid());

        QSignalSpy gotFrameSpy(remoteView, &RemoteViewInterface::frameUpdated);
        QVERIFY(gotFrameSpy.isValid());

        QVERIFY(showSource(QStringLiteral("qrc:/manual/rotationinvariant.qml")));

        remoteView->clientViewUpdated();
        if (!isViewExposed())
            return;

        QVERIFY(gotFrameSpy.wait() || gotFrameSpy.size() >= 1);

        QVERIFY(!renderSpy.isEmpty());
        QVERIFY(!gotFrameSpy.isEmpty());
        // take the last frame
        const auto last = gotFrameSpy.size() - 1;
        const auto frame = gotFrameSpy.at(last).at(0).value<RemoteViewFrame>();
        QImage img = frame.image();
        QTransform transform = frame.transform();

        img = img.transformed(transform);

        QVERIFY(!img.isNull());
        QCOMPARE(img.width(), static_cast<int>(view()->width() * view()->devicePixelRatio()));
        QCOMPARE(img.height(), static_cast<int>(view()->height() * view()->devicePixelRatio()));

        // Grabbed stuff seems to alter colors depending the monitor color profile, let use plain QColor for comparison.
        QCOMPARE(QColor(img.pixel(1 * view()->devicePixelRatio(), 1 * view()->devicePixelRatio())), QColor(255, 0, 0));
        QCOMPARE(QColor(img.pixel(99 * view()->devicePixelRatio(), 1 * view()->devicePixelRatio())), QColor(0, 255, 0));
        QCOMPARE(QColor(img.pixel(1 * view()->devicePixelRatio(), 99 * view()->devicePixelRatio())), QColor(0, 0, 255));
        QCOMPARE(QColor(img.pixel(99 * view()->devicePixelRatio(), 99 * view()->devicePixelRatio())), QColor(255, 255, 0));

        remoteView->setViewActive(false);
    }

    void testCustomRenderModes()
    {
        QSignalSpy featureSpy(inspector, &QuickInspectorInterface::features);
        QVERIFY(featureSpy.isValid());
        inspector->checkFeatures();
        QCOMPARE(featureSpy.size(), 1);
        auto features = featureSpy.at(0).at(0).value<GammaRay::QuickInspectorInterface::Features>();

        QSignalSpy renderSpy(view(), &QQuickWindow::frameSwapped);
        QVERIFY(renderSpy.isValid());

        QVERIFY(showSource(QStringLiteral("qrc:/manual/reparenttest.qml")));

        if (features & QuickInspectorInterface::CustomRenderModeClipping) {
            // We can't do more than making sure, it doesn't crash. Let's wait some frames
            inspector->setCustomRenderMode(QuickInspectorInterface::VisualizeClipping);
            for (int i = 0; i < 3; i++)
                triggerSceneChange();
            if (isViewExposed())
                QVERIFY(waitForSignal(&renderSpy));
        }

        if (features & QuickInspectorInterface::CustomRenderModeOverdraw) {
            inspector->setCustomRenderMode(QuickInspectorInterface::VisualizeOverdraw);
            for (int i = 0; i < 3; i++)
                triggerSceneChange();
            if (isViewExposed())
                QVERIFY(waitForSignal(&renderSpy));
        }

        if (features & QuickInspectorInterface::CustomRenderModeBatches) {
            inspector->setCustomRenderMode(QuickInspectorInterface::VisualizeBatches);
            for (int i = 0; i < 3; i++)
                triggerSceneChange();
            if (isViewExposed())
                QVERIFY(waitForSignal(&renderSpy));
        }

        if (features & QuickInspectorInterface::CustomRenderModeChanges) {
            inspector->setCustomRenderMode(QuickInspectorInterface::VisualizeChanges);
            for (int i = 0; i < 3; i++)
                triggerSceneChange();
            if (isViewExposed())
                QVERIFY(waitForSignal(&renderSpy));
        }

        inspector->setCustomRenderMode(QuickInspectorInterface::NormalRendering);
        for (int i = 0; i < 3; i++)
            triggerSceneChange();
        if (isViewExposed())
            QVERIFY(waitForSignal(&renderSpy));
    }

    void testAnchorsPropertyFilter()
    {
        PropertyData testData;
        testData.setName("something");
        testData.setClassName("QQuickItem");
        testData.setTypeName("QQuickAnchors");
        QVERIFY(!PropertyFilters::matches(testData));
        testData.setName("anchors");
        QVERIFY(PropertyFilters::matches(testData));

        QVERIFY(showSource(QStringLiteral("qrc:/manual/anchorspropertyfiltertest.qml")));

        auto rectWithoutAnchors = view()->rootObject()->findChild<QQuickItem *>("rectWithoutAnchors");
        auto rectWithAnchors = view()->rootObject()->findChild<QQuickItem *>("rectWithAnchors");

        auto rectWithoutAnchorsPriv = QQuickItemPrivate::get(rectWithoutAnchors);
        auto rectWithAnchorsPriv = QQuickItemPrivate::get(rectWithAnchors);

        QVERIFY(!rectWithoutAnchorsPriv->_anchors);
        QVERIFY(rectWithAnchorsPriv->_anchors);

        auto propertyModel = ObjectBroker::model("com.kdab.GammaRay.ObjectInspector.properties");
        QVERIFY(propertyModel);
        QSortFilterProxyModel anchorsFilterModel;
        anchorsFilterModel.setSourceModel(propertyModel);
        anchorsFilterModel.setFilterKeyColumn(0);
        anchorsFilterModel.setFilterFixedString("anchors");

        Probe::instance()->selectObject(rectWithoutAnchors);
        QVERIFY(propertyModel->rowCount());
        QCOMPARE(anchorsFilterModel.rowCount(), 1);
        auto rectWithoutAnchorsAnchorsValue = anchorsFilterModel.data(anchorsFilterModel.index(0, 1), Qt::EditRole);
        QVERIFY(rectWithoutAnchorsAnchorsValue.canConvert<QObject *>());
        QVERIFY(rectWithoutAnchorsAnchorsValue.value<QObject *>() == nullptr);

        Probe::instance()->selectObject(rectWithAnchors);
        QCOMPARE(anchorsFilterModel.rowCount(), 1);
        auto rectWithAnchorsAnchorsValue = anchorsFilterModel.data(anchorsFilterModel.index(0, 1), Qt::EditRole);
        QVERIFY(rectWithAnchorsAnchorsValue.canConvert<QObject *>());
        QVERIFY(rectWithAnchorsAnchorsValue.value<QObject *>() != nullptr);


        Probe::instance()->selectObject(rectWithoutAnchors);
        // We want to trigger as much QuiickInspector-code as possible to check that
        // QQuickItemPrivate::anchors is not called by any GammaRay-code as that
        // would render the filter useless.
        auto remoteView =
            ObjectBroker::object<RemoteViewInterface *>(
                QStringLiteral("com.kdab.GammaRay.QuickRemoteView"));

        QVERIFY(remoteView);
        remoteView->setViewActive(true);
        remoteView->clientViewUpdated();
        QTest::qWait(10);

        rectWithoutAnchorsAnchorsValue = anchorsFilterModel.data(anchorsFilterModel.index(0, 1), Qt::EditRole);
        QVERIFY(rectWithoutAnchorsAnchorsValue.canConvert<QObject *>());
        QVERIFY(rectWithoutAnchorsAnchorsValue.value<QObject *>() == nullptr);
    }

    void testProblemReporting()
    {
        // TODO using this qml-file as testcase might stop working if qt decides to be
        //  smarter with out of view items in ListViews
        QVERIFY(showSource(QStringLiteral("qrc:/manual/quickitemcreatedestroytest.qml")));

        QVERIFY(ProblemCollector::instance()->isCheckerRegistered("com.kdab.GammaRay.QuickItemChecker"));

        ProblemCollector::instance()->requestScan();
        if (!isViewExposed()) { // if the CI fails to show the window, this isn't going to succeed
            return;
        }

        const auto &problems = ProblemCollector::instance()->problems();
        QVERIFY(std::any_of(problems.begin(), problems.end(),
                            [&](const Problem &p) {
                                return p.problemId.startsWith("com.kdab.GammaRay.QuickItemChecker")
                                    && !p.object.isNull()
                                    && p.description.contains("out of view")
                                    && p.locations.size() > 0;
                            }));
    }

private:
    QAbstractItemModel *itemModel;
    QAbstractItemModel *sgModel;
    QuickInspectorInterface *inspector;
};

QTEST_MAIN(QuickInspectorTest)

#include "quickinspectortest.moc"
