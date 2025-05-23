/*
  stacktracemodel.cpp

  This file is part of GammaRay, the Qt application inspection and manipulation tool.

  SPDX-FileCopyrightText: 2017 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: Volker Krause <volker.krause@kdab.com>

  SPDX-License-Identifier: GPL-2.0-or-later

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "stacktracemodel.h"

#include <QDebug>

using namespace GammaRay;

StackTraceModel::StackTraceModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

StackTraceModel::~StackTraceModel() = default;

void StackTraceModel::setStackTrace(const Execution::Trace &trace)
{
    if (!m_trace.empty()) {
        beginRemoveRows(QModelIndex(), 0, m_trace.size() - 1);
        m_frames.clear();
        m_trace = Execution::Trace();
        endRemoveRows();
    }

    if (!trace.empty()) {
        beginInsertRows(QModelIndex(), 0, trace.size() - 1);
        m_trace = trace;
        m_frames.clear();
        endInsertRows();
    }
}

int StackTraceModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 2;
}

int StackTraceModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_trace.size();
}

QVariant StackTraceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (m_trace.size() && !m_frames.size()) {
        m_frames = Execution::resolveAll(m_trace);
    }

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0:
            return m_frames.at(index.row()).name;
        case 1:
            return QVariant::fromValue(m_frames.at(index.row()).location);
        }
    }

    return QVariant();
}

QVariant StackTraceModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case 0:
            return tr("Function");
        case 1:
            return tr("Location");
        }
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

QStringList StackTraceModel::fullTrace() const
{
    QStringList bt;
    bt.reserve(m_frames.size());
    for (const auto &frame : std::as_const(m_frames)) {
        if (frame.location.isValid())
            bt.push_back(frame.name + QLatin1String(" (") + frame.location.displayString() + QLatin1Char(')'));
        else
            bt.push_back(frame.name);
    }

    return bt;
}
