/*
  propertyaggregator.cpp

  This file is part of GammaRay, the Qt application inspection and manipulation tool.

  SPDX-FileCopyrightText: 2015 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: Volker Krause <volker.krause@kdab.com>

  SPDX-License-Identifier: GPL-2.0-or-later

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "propertyaggregator.h"
#include "propertydata.h"
#include "objectinstance.h"

#include <QDebug>

#include <algorithm>
#include <numeric>

using namespace GammaRay;

PropertyAggregator::PropertyAggregator(QObject *parent)
    : PropertyAdaptor(parent)
{
}

PropertyAggregator::~PropertyAggregator() = default;

void PropertyAggregator::doSetObject(const ObjectInstance &oi)
{
    std::for_each(m_propertyAdaptors.begin(), m_propertyAdaptors.end(), [&oi](PropertyAdaptor *pa) {
        pa->setObject(oi);
    });
}

int PropertyAggregator::count() const
{
    if (!object().isValid())
        return 0;
    return std::accumulate(m_propertyAdaptors.constBegin(), m_propertyAdaptors.constEnd(), 0,
                           [](int lhs, PropertyAdaptor *rhs) {
                               return lhs + rhs->count();
                           });
}

PropertyData PropertyAggregator::propertyData(int index) const
{
    if (!object().isValid())
        return PropertyData();

    int offset = 0;
    for (const auto adaptor : m_propertyAdaptors) {
        if (index < offset + adaptor->count())
            return adaptor->propertyData(index - offset);
        offset += adaptor->count();
    }

    Q_ASSERT(false);
    return PropertyData();
}

void PropertyAggregator::writeProperty(int index, const QVariant &value)
{
    if (!object().isValid())
        return;

    int offset = 0;
    for (const auto adaptor : std::as_const(m_propertyAdaptors)) {
        if (index < offset + adaptor->count()) {
            QPointer<PropertyAggregator> guard(this);
            adaptor->writeProperty(index - offset, value);
            if (guard) {
                m_oi = adaptor->object(); // propagate changes back to us, particularly matters for value types
            }
            return;
        }
        offset += adaptor->count();
    }

    Q_ASSERT(false);
}

bool PropertyAggregator::canAddProperty() const
{
    auto count = std::count_if(m_propertyAdaptors.constBegin(), m_propertyAdaptors.constEnd(),
                               [](PropertyAdaptor *pa) {
                                   return pa->canAddProperty();
                               });
    return count == 1;
}

void PropertyAggregator::addProperty(const PropertyData &data)
{
    if (!object().isValid())
        return;

    Q_ASSERT(canAddProperty());

    for (const auto adaptor : std::as_const(m_propertyAdaptors)) {
        if (adaptor->canAddProperty()) {
            adaptor->addProperty(data);
            return;
        }
    }

    Q_ASSERT(false);
}

void PropertyAggregator::resetProperty(int index)
{
    if (!object().isValid())
        return;

    int offset = 0;
    for (const auto adaptor : std::as_const(m_propertyAdaptors)) {
        if (index < offset + adaptor->count()) {
            adaptor->resetProperty(index - offset);
            return;
        }
        offset += adaptor->count();
    }

    Q_ASSERT(false);
}

void PropertyAggregator::addPropertyAdaptor(PropertyAdaptor *adaptor)
{
    m_propertyAdaptors.push_back(adaptor);
    connect(adaptor, &PropertyAdaptor::propertyChanged, this, &PropertyAggregator::slotPropertyChanged);
    connect(adaptor, &PropertyAdaptor::propertyAdded, this, &PropertyAggregator::slotPropertyAdded);
    connect(adaptor, &PropertyAdaptor::propertyRemoved, this, &PropertyAggregator::slotPropertyRemoved);
    connect(adaptor, &PropertyAdaptor::objectInvalidated, this, &PropertyAdaptor::objectInvalidated);
}

void PropertyAggregator::slotPropertyChanged(int first, int last)
{
    auto source = sender();
    Q_ASSERT(source);

    int offset = 0;
    for (auto pa : std::as_const(m_propertyAdaptors)) {
        if (pa == source) {
            emit propertyChanged(first + offset, last + offset);
            return;
        }
        offset += pa->count();
    }
}

void PropertyAggregator::slotPropertyAdded(int first, int last)
{
    auto source = sender();
    Q_ASSERT(source);

    int offset = 0;
    for (auto pa : std::as_const(m_propertyAdaptors)) {
        if (pa == source) {
            emit propertyAdded(first + offset, last + offset);
            return;
        }
        offset += pa->count();
    }
}

void PropertyAggregator::slotPropertyRemoved(int first, int last)
{
    auto source = sender();
    Q_ASSERT(source);

    int offset = 0;
    for (auto pa : std::as_const(m_propertyAdaptors)) {
        if (pa == source) {
            emit propertyRemoved(first + offset, last + offset);
            return;
        }
        offset += pa->count();
    }
}
