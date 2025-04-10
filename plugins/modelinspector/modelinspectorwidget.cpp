/*
  modelinspectorwidget.cpp

  This file is part of GammaRay, the Qt application inspection and manipulation tool.

  SPDX-FileCopyrightText: 2010 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: Volker Krause <volker.krause@kdab.com>

  SPDX-License-Identifier: GPL-2.0-or-later

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "modelinspectorwidget.h"
#include "ui_modelinspectorwidget.h"
#include "modelinspectorclient.h"
#include "modelcontentdelegate.h"

#include <ui/contextmenuextension.h>
#include <ui/itemdelegate.h>
#include <ui/searchlinecontroller.h>
#include <ui/propertyeditor/propertyeditordelegate.h>

#include <core/metaenum.h>

#include <common/endpoint.h>
#include <common/objectbroker.h>
#include <common/objectmodel.h>

#include <QDebug>
#include <QMenu>

using namespace GammaRay;

static QObject *createModelInspectorClient(const QString & /*name*/, QObject *parent)
{
    return new ModelInspectorClient(parent);
}

ModelInspectorWidget::ModelInspectorWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ModelInspectorWidget)
    , m_stateManager(this)
    , m_interface(nullptr)
{
    ui->setupUi(this);

    ui->modelView->header()->setObjectName("modelViewHeader");
    ui->modelView->setDeferredResizeMode(0, QHeaderView::ResizeToContents);
    connect(ui->modelView, &QWidget::customContextMenuRequested,
            this, &ModelInspectorWidget::modelContextMenu);

    auto selectionModels = ObjectBroker::model(QStringLiteral("com.kdab.GammaRay.SelectionModels"));
    ui->selectionModelsView->setModel(selectionModels);
    ui->selectionModelsView->header()->setObjectName("selectionModelsViewHeader");
    ui->selectionModelsView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    connect(ui->selectionModelsView, &QWidget::customContextMenuRequested,
            this, &ModelInspectorWidget::selectionModelContextMenu);
    ui->selectionModelsView->setSelectionModel(ObjectBroker::selectionModel(selectionModels));

    auto contentModel = ObjectBroker::model(QStringLiteral("com.kdab.GammaRay.ModelContent"));
    ui->modelContentView->setModel(contentModel);
    ui->modelContentView->setSelectionModel(ObjectBroker::selectionModel(contentModel));
    ui->modelContentView->header()->setObjectName("modelContentViewHeader");
    ui->modelContentView->setItemDelegate(new ModelContentDelegate(this));

    ui->modelCellView->header()->setObjectName("modelCellViewHeader");
    ui->modelCellView->setItemDelegate(new PropertyEditorDelegate(this));

    ObjectBroker::registerClientObjectFactoryCallback<ModelInspectorInterface *>(
        createModelInspectorClient);
    m_interface = ObjectBroker::object<ModelInspectorInterface *>();
    connect(m_interface, &ModelInspectorInterface::currentCellDataChanged, this, &ModelInspectorWidget::cellDataChanged);

    auto modelModel = ObjectBroker::model(QStringLiteral("com.kdab.GammaRay.ModelModel"));
    ui->modelView->setModel(modelModel);
    ui->modelView->setSelectionModel(ObjectBroker::selectionModel(modelModel));
    new SearchLineController(ui->modelSearchLine, modelModel);
    connect(ui->modelView->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this, &ModelInspectorWidget::modelSelected);

    ui->modelCellView->setModel(ObjectBroker::model(QStringLiteral(
        "com.kdab.GammaRay.ModelCellModel")));

    m_stateManager.setDefaultSizes(ui->mainSplitter, UISizeVector() << "33%"
                                                                    << "33%"
                                                                    << "33%");

    cellDataChanged();
}

ModelInspectorWidget::~ModelInspectorWidget() = default;

void ModelInspectorWidget::modelSelected(const QItemSelection &selected)
{
    QModelIndex index;
    if (!selected.isEmpty())
        index = selected.first().topLeft();

    if (index.isValid())
        // in case selection is not directly triggered by the user
        ui->modelView->scrollTo(index, QAbstractItemView::EnsureVisible);
}

#define F(x)      \
    {             \
        Qt::x, #x \
    }
static const MetaEnum::Value<Qt::ItemFlag> item_flag_table[] = {
    { Qt::ItemIsSelectable, "Selectable" },
    { Qt::ItemIsEditable, "Editable" },
    { Qt::ItemIsDragEnabled, "DragEnabled" },
    { Qt::ItemIsDropEnabled, "DropEnabled" },
    { Qt::ItemIsUserCheckable, "UserCheckable" },
    { Qt::ItemIsEnabled, "Enabled" },
    { Qt::ItemIsAutoTristate, "AutoTristate" },
    { Qt::ItemNeverHasChildren, "ItemNeverHasChildren" },
    { Qt::ItemIsUserTristate, "UserTristate" }
};
#undef F

void ModelInspectorWidget::cellDataChanged()
{
    const auto cellData = m_interface->currentCellData();
    ui->indexLineEdit->setText(cellData.row != -1
                                   ? tr("Row: %1 Column: %2").arg(cellData.row).arg(cellData.column)
                                   : tr("Invalid"));
    ui->internalIdLineEdit->setText(cellData.internalId);
    ui->internalPtrLineEdit->setText(cellData.internalPtr);
    ui->flagsPlainTextEdit->selectAll();
    ui->flagsPlainTextEdit->insertPlainText(MetaEnum::flagsToString(cellData.flags, item_flag_table));
}

void ModelInspectorWidget::objectRegistered(const QString &objectName)
{
    if (objectName == QLatin1String("com.kdab.GammaRay.ModelContent.selection"))
        // delay, since it's not registered yet when the signal is emitted
        QMetaObject::invokeMethod(this, "setupModelContentSelectionModel", Qt::QueuedConnection);
}

void ModelInspectorWidget::modelContextMenu(QPoint pos)
{
    const auto index = ui->modelView->indexAt(pos);
    if (!index.isValid())
        return;

    const auto objectId = index.data(ObjectModel::ObjectIdRole).value<ObjectId>();
    QMenu menu;
    ContextMenuExtension ext(objectId);
    ext.setLocation(ContextMenuExtension::Creation, index.data(ObjectModel::CreationLocationRole).value<SourceLocation>());
    ext.setLocation(ContextMenuExtension::Declaration,
                    index.data(ObjectModel::DeclarationLocationRole).value<SourceLocation>());
    ext.populateMenu(&menu);

    menu.exec(ui->modelView->viewport()->mapToGlobal(pos));
}

void ModelInspectorWidget::selectionModelContextMenu(QPoint pos)
{
    const auto index = ui->selectionModelsView->indexAt(pos);
    if (!index.isValid())
        return;

    const auto objectId = index.data(ObjectModel::ObjectIdRole).value<ObjectId>();
    QMenu menu;
    ContextMenuExtension ext(objectId);
    ext.setLocation(ContextMenuExtension::Creation, index.data(ObjectModel::CreationLocationRole).value<SourceLocation>());
    ext.setLocation(ContextMenuExtension::Declaration, index.data(ObjectModel::DeclarationLocationRole).value<SourceLocation>());
    ext.populateMenu(&menu);

    menu.exec(ui->selectionModelsView->viewport()->mapToGlobal(pos));
}
