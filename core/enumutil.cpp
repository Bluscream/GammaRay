/*
  enumutil.cpp

  This file is part of GammaRay, the Qt application inspection and manipulation tool.

  SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: Volker Krause <volker.krause@kdab.com>

  SPDX-License-Identifier: GPL-2.0-or-later

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "enumutil.h"
#include "enumrepositoryserver.h"

#include <QDebug>
#include <QMetaEnum>

using namespace GammaRay;

static const QMetaObject *metaObjectForClass(const QByteArray &name)
{
    if (name.isEmpty())
        return nullptr;
    auto mo = QMetaType::fromName(name).metaObject();
    // auto mo = QMetaType::metaObjectForType(QMetaType::type(name));
    if (mo)
        return mo;
    mo = QMetaType::fromName(name + '*').metaObject(); // try pointer version, more likely for QObjects
    return mo;
}

static QMetaEnum flagsFromEnumIndex(int enumIndex, const QByteArray &enumName, const QMetaObject *const mo)
{
    Q_ASSERT(mo);
    // usually it should be just enumIndex + 1
    const auto count = mo->enumeratorCount();
    if (enumIndex + 1 < count) {
        const QMetaEnum me = mo->enumerator(enumIndex + 1);
        if (me.isFlag() && enumName == me.enumName()) {
            return me;
        }
    }

    for (int i = mo->enumeratorOffset(); i < count; ++i) {
        const QMetaEnum me = mo->enumerator(i);
        if (me.isFlag() && enumName == me.enumName()) {
            return me;
        }
    }
    return mo->enumerator(enumIndex);
}

QMetaEnum EnumUtil::metaEnum(const QVariant &value, const char *typeName, const QMetaObject *metaObject)
{
    QByteArray fullTypeName(typeName);
    if (fullTypeName.isEmpty())
        fullTypeName = value.typeName();

    // split class name and enum name
    QByteArray className;
    QByteArray enumTypeName(fullTypeName);
    const int pos = enumTypeName.lastIndexOf("::");
    bool isFlag = false;
    if (pos >= 0) {
        className = enumTypeName.left(pos);
        enumTypeName = enumTypeName.mid(pos + 2);

        // QVariant::typeName is no longer the flag name, but QFlags<Foo>...
        if (className.startsWith("QFlags<")) {
            className.remove(0, sizeof("QFlags<") - 1);
            isFlag = true;
        }

        if (enumTypeName.endsWith(">")) {
            enumTypeName.chop(1);
        }
    }

    const QMetaObject *mo = &Qt::staticMetaObject;
    int enumIndex = mo->indexOfEnumerator(enumTypeName);
    if (enumIndex < 0 && metaObject) {
        mo = metaObject;
        enumIndex = mo->indexOfEnumerator(enumTypeName);
    }
    if (enumIndex < 0 && (mo = QMetaType::fromName(fullTypeName).metaObject())) {
        enumIndex = mo->indexOfEnumerator(enumTypeName);
    }
    if (enumIndex < 0 && (mo = metaObjectForClass(className))) {
        enumIndex = mo->indexOfEnumerator(enumTypeName);
    }

    // attempt to recover namespaces from semi-qualified type names
    if (enumIndex < 0 && metaObject) {
        QByteArray n(metaObject->className());
        const int pos = n.lastIndexOf("::");
        if (pos > 0) {
            n = n.left(pos + 2) + fullTypeName;
            return metaEnum(value, n, nullptr);
        }
    }

    if (enumIndex < 0)
        return {};

    if (isFlag) {
        return flagsFromEnumIndex(enumIndex, enumTypeName, mo);
    }

    return mo->enumerator(enumIndex);
}

int EnumUtil::enumToInt(const QVariant &value, const QMetaEnum &metaEnum)
{
    // QVariant has no implicit QFlag to int conversion as of Qt 5.7
    if (metaEnum.isFlag() && QMetaType(value.userType()).sizeOf() == sizeof(int)) // int should be enough, QFlag has that hardcoded
        return value.constData() ? *static_cast<const int *>(value.constData()) : 0;
    return value.toInt();
}

QString EnumUtil::enumToString(const QVariant &value, const char *typeName, const QMetaObject *metaObject)
{
    const auto me = metaEnum(value, typeName, metaObject);
    if (me.isValid()) {
        if (me.isFlag()) {
            return QString::fromUtf8(me.valueToKeys(enumToInt(value, me)));
        } else {
            return QString::fromUtf8(me.valueToKey(enumToInt(value, me)));
        }
    }
    if (EnumRepositoryServer::isEnum(value.userType())) {
        const auto ev = EnumRepositoryServer::valueFromVariant(value);
        const auto def = EnumRepositoryServer::definitionForId(ev.id());
        return def.valueToString(ev);
    }
    return QString();
}
