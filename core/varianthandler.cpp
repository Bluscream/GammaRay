/*
  varianthandler.cpp

  This file is part of GammaRay, the Qt application inspection and manipulation tool.

  SPDX-FileCopyrightText: 2013 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: Volker Krause <volker.krause@kdab.com>

  SPDX-License-Identifier: GPL-2.0-or-later

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "varianthandler.h"
#include "util.h"
#include "enumutil.h"
#include "enumrepositoryserver.h"

#include <common/metatypedeclarations.h>

#include <QGuiApplication>

#include <QAssociativeIterable>
#include <QCursor>
#include <QDebug>
#include <QDir>
#include <QEasingCurve>
#include <QIcon>
#include <QMatrix4x4>
#include <QMetaEnum>
#include <QMetaObject>
#include <QObject>
#include <QPainter>
#include <QPalette>
#include <QPoint>
#include <QRect>
#include <QSequentialIterable>
#include <QSize>
#include <QStringList>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QJsonObject>
#include <QJsonArray>

Q_DECLARE_METATYPE(const QObject *)

using namespace GammaRay;

namespace GammaRay {
class VariantHandlerRepository
{
public:
    VariantHandlerRepository() = default;
    ~VariantHandlerRepository();
    void clear();

    QHash<int, VariantHandler::Converter<QString> *> stringConverters;
    QVector<VariantHandler::GenericStringConverter> genericStringConverters;

private:
    Q_DISABLE_COPY(VariantHandlerRepository)
};

VariantHandlerRepository::~VariantHandlerRepository()
{
    qDeleteAll(stringConverters);
}

void VariantHandlerRepository::clear()
{
    qDeleteAll(stringConverters);
    stringConverters.clear();
    genericStringConverters.clear();
}

static QString displayMatrix4x4(const QMatrix4x4 &matrix)
{
    QStringList rows;
    rows.reserve(4);
    for (int i = 0; i < 4; ++i) {
        QStringList cols;
        cols.reserve(4);
        for (int j = 0; j < 4; ++j) {
            cols.push_back(QString::number(matrix(i, j)));
        }
        rows.push_back(cols.join(QStringLiteral(" ")));
    }
    return '[' + rows.join(QStringLiteral(", ")) + ']';
}

static QString displayMatrix4x4(const QMatrix4x4 *matrix)
{
    if (matrix) {
        return displayMatrix4x4(*matrix);
    }
    return QStringLiteral("<null>");
}

template<int Dim, typename T>
static QString displayVector(const T &vector)
{
    QStringList v;
    for (int i = 0; i < Dim; ++i) {
        v.push_back(QString::number(vector[i]));
    }
    return '[' + v.join(QStringLiteral(", ")) + ']';
}

static bool isEnum(const QVariant &value)
{
    if (!value.isValid())
        return false;
    const QMetaType mt(value.userType());
    return mt.isValid() && ((mt.flags() & QMetaType::IsEnumeration) || (mt.flags() & QMetaType::IsUnsignedEnumeration));
}
}

Q_GLOBAL_STATIC(VariantHandlerRepository, s_variantHandlerRepository)

QString VariantHandler::displayString(const QVariant &value)
{
    switch (value.typeId()) {
#ifndef QT_NO_CURSOR
    case QMetaType::QCursor: {
        const QCursor cursor = value.value<QCursor>();
        return EnumUtil::enumToString(QVariant::fromValue<int>(cursor.shape()), "Qt::CursorShape");
    }
#endif
    case QMetaType::QIcon: {
        const QIcon icon = value.value<QIcon>();
        if (icon.isNull()) {
            return qApp->translate("GammaRay::VariantHandler", "<no icon>");
        }
        const auto sizes = icon.availableSizes();
        QStringList l;
        l.reserve(sizes.size());
        for (QSize size : sizes) {
            l.push_back(displayString(size));
        }
        return l.join(QStringLiteral(", "));
    }
    case QMetaType::QLine: {
        const auto line = value.toLine();
        return QStringLiteral("%1, %2 → %3, %4").arg(line.x1()).arg(line.y1()).arg(line.x2()).arg(line.y2());
    }

    case QMetaType::QLineF: {
        const auto line = value.toLineF();
        return QStringLiteral("%1, %2 → %3, %4").arg(line.x1()).arg(line.y1()).arg(line.x2()).arg(line.y2());
    }

    case QMetaType::QLocale:
        return value.toLocale().name();

    case QMetaType::QPoint: {
        const auto point = value.toPoint();
        return QStringLiteral("%1, %2").arg(point.x()).arg(point.y());
    }

    case QMetaType::QPointF: {
        const auto point = value.toPointF();
        return QStringLiteral("%1, %2").arg(point.x()).arg(point.y());
    }

    case QMetaType::QRect: {
        const auto rect = value.toRect();
        return QStringLiteral("%1, %2 %3 x %4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
    }

    case QMetaType::QRectF: {
        const auto rect = value.toRectF();
        return QStringLiteral("%1, %2 %3 x %4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
    }

    case QMetaType::QPalette: {
        const QPalette pal = value.value<QPalette>();
        if (pal == qApp->palette()) {
            return QStringLiteral("<inherited>");
        }
        return QStringLiteral("<custom>");
    }

    case QMetaType::QSize: {
        const auto size = value.toSize();
        return QStringLiteral("%1 x %2").arg(size.width()).arg(size.height());
    }

    case QMetaType::QSizeF: {
        const auto size = value.toSizeF();
        return QStringLiteral("%1 x %2").arg(size.width()).arg(size.height());
    }

    case QMetaType::QStringList: {
        const auto l = value.toStringList();
        if (l.isEmpty()) {
            return QStringLiteral("<empty>");
        }
        if (l.size() == 1) {
            return l.at(0);
        }
        return QStringLiteral("<%1 entries>").arg(l.size());
    }

    case QMetaType::QTransform: {
        const QTransform t = value.value<QTransform>();
        return QStringLiteral("[%1 %2 %3, %4 %5 %6, %7 %8 %9]").arg(t.m11()).arg(t.m12()).arg(t.m13()).arg(t.m21()).arg(t.m22()).arg(t.m23()).arg(t.m31()).arg(t.m32()).arg(t.m33());
    }
    default:
        break;
    }

    // types with dynamic type ids
    if (value.userType() == qMetaTypeId<uchar>()) {
        const auto v = value.value<uchar>();
        return QString::number(v) + QLatin1String(" '") + QChar(v) + QLatin1Char('\'');
    }

    if (value.userType() == qMetaTypeId<const QMetaObject *>()) {
        const auto mo = value.value<const QMetaObject *>();
        if (!mo) {
            return QStringLiteral("0x0");
        }
        return mo->className();
    }

    if (value.userType() == qMetaTypeId<QMatrix4x4>())
        return displayMatrix4x4(value.value<QMatrix4x4>());

    if (value.userType() == qMetaTypeId<const QMatrix4x4 *>())
        return displayMatrix4x4(value.value<const QMatrix4x4 *>());

    if (value.userType() == qMetaTypeId<QVector2D>()) {
        return displayVector<2>(value.value<QVector2D>());
    }
    if (value.userType() == qMetaTypeId<QVector3D>()) {
        return displayVector<3>(value.value<QVector3D>());
    }
    if (value.userType() == qMetaTypeId<QVector4D>()) {
        return displayVector<4>(value.value<QVector4D>());
    }
    if (value.userType() == qMetaTypeId<QTimeZone>()) {
        return value.value<QTimeZone>().id();
    }

    if (value.userType() == qMetaTypeId<QSet<QByteArray>>()) {
        const QSet<QByteArray> set = value.value<QSet<QByteArray>>();
        QStringList l;
        l.reserve(set.size());
        for (const QByteArray &b : set) {
            l.push_back(QString::fromUtf8(b));
        }
        return l.join(QStringLiteral(", "));
    }

    if (value.userType() == qMetaTypeId<QEasingCurve>()) {
        const auto ec = value.toEasingCurve();
        return EnumUtil::enumToString(QVariant::fromValue<QEasingCurve::Type>(ec.type()));
    }

    // enums
    const QString enumStr = EnumUtil::enumToString(value);
    if (!enumStr.isEmpty()) {
        return enumStr;
    }

    // custom converters
    auto it = s_variantHandlerRepository()->stringConverters.constFind(value.userType());
    if (it != s_variantHandlerRepository()->stringConverters.constEnd()) {
        return (*it.value())(value);
    }

    // Work around QTBUG-73437
    if (value.userType() == qMetaTypeId<QJsonObject>()) {
        int size = value.value<QJsonObject>().size();
        if (size == 0) {
            return QStringLiteral("<empty>");
        } else {
            return QStringLiteral("<%1 entries>").arg(size);
        }
    }

    if (value.userType() == qMetaTypeId<QJsonArray>()) {
        int size = value.value<QJsonArray>().size();
        if (size == 0) {
            return QStringLiteral("<empty>");
        } else {
            return QStringLiteral("<%1 entries>").arg(size);
        }
    }

    if (value.userType() == qMetaTypeId<QJsonValue>()) {

        QJsonValue v = value.value<QJsonValue>();

        if (v.isBool()) {
            return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        } else if (v.isDouble()) {
            return QString::number(v.toDouble());
        } else if (v.isNull()) {
            return QStringLiteral("null");
        } else if (v.isArray()) {
            int size = v.toArray().size();
            if (size == 0) {
                return QStringLiteral("<empty>");
            } else {
                return QStringLiteral("<%1 entries>").arg(size);
            }
        } else if (v.isObject()) {
            int size = v.toObject().size();
            if (size == 0) {
                return QStringLiteral("<empty>");
            } else {
                return QStringLiteral("<%1 entries>").arg(size);
            }
        } else if (v.isString()) {
            return v.toString();
        } else {
            return QStringLiteral("undefined");
        }
    }

    if (value.canConvert<QVariantList>() && !value.canConvert<QString>()) {
        QSequentialIterable it = value.value<QSequentialIterable>();
        if (it.size() == 0) {
            return QStringLiteral("<empty>");
        } else {
            return QStringLiteral("<%1 entries>").arg(it.size());
        }
    }
    if (value.canConvert<QVariantHash>()) {
        auto it = value.value<QAssociativeIterable>();
        if (it.size() == 0) {
            return QStringLiteral("<empty>");
        } else {
            return QStringLiteral("<%1 entries>").arg(it.size());
        }
    }

    // generic converters
    const QVector<VariantHandler::GenericStringConverter> genStrConverters = s_variantHandlerRepository()->genericStringConverters;
    for (auto converter : genStrConverters) {
        bool ok = false;
        const QString s = converter(value, &ok);
        if (ok)
            return s;
    }

    // catch-all QObject handler
    // search the entire hierarchy for custom converters, so we can override this
    // for entire sub-trees
    if (value.canConvert<QObject *>() || value.canConvert<const QObject *>()) {
        bool isConst = value.canConvert<const QObject *>();
        const auto obj = isConst ? value.value<const QObject *>() : value.value<QObject *>();
        if (!obj || obj->metaObject() == &QObject::staticMetaObject)
            return Util::displayString(obj);

        auto mo = obj->metaObject();
        while (mo) {
            auto name = QByteArray(mo->className()) + '*';
            auto type = QMetaType::fromName(name).id();
            if (type > 0) {
                auto it = s_variantHandlerRepository()->stringConverters.constFind(type);
                if (it != s_variantHandlerRepository()->stringConverters.constEnd())
                    return (*it.value())(value);
            }
            mo = mo->superClass();
        }
        return Util::displayString(obj);
    }

    if (isEnum(value)) {
        return QString::number(value.toInt());
    }

    return value.toString();
}

QVariant VariantHandler::decoration(const QVariant &value)
{
    switch (value.typeId()) {
    case QMetaType::QBrush: {
        const QBrush b = value.value<QBrush>();
        if (b.style() != Qt::NoBrush) {
            QPixmap p(16, 16);
            p.fill(QColor(0, 0, 0, 0));
            QPainter painter(&p);
            painter.setBrush(b);
            painter.drawRect(0, 0, p.width() - 1, p.height() - 1);
            return p;
        }
        break;
    }
    case QMetaType::QColor: {
        const QColor c = value.value<QColor>();
        if (c.isValid()) {
            QPixmap p(16, 16);
            QPainter painter(&p);
            Util::drawTransparencyPattern(&painter, p.rect(), 4);
            painter.setBrush(QBrush(c));
            painter.drawRect(0, 0, p.width() - 1, p.height() - 1);
            return p;
        }
        break;
    }
#ifndef QT_NO_CURSOR
    case QMetaType::QCursor: {
        const QCursor c = value.value<QCursor>();
        if (!c.pixmap().isNull()) {
            return c.pixmap().scaled(16, 16, Qt::KeepAspectRatio, Qt::FastTransformation);
        }
        break;
    }
#endif
    case QMetaType::QIcon:
        return value;
    case QMetaType::QPen: {
        const QPen pen = value.value<QPen>();
        if (pen.style() != Qt::NoPen) {
            QPixmap p(16, 16);
            QPainter painter(&p);
            Util::drawTransparencyPattern(&painter, p.rect(), 4);
            painter.save();
            painter.setPen(pen);
            painter.translate(0, 8 - pen.width() / 2);
            painter.drawLine(0, 0, p.width(), 0);
            painter.restore();
            painter.drawRect(0, 0, p.width() - 1, p.height() - 1);
            return p;
        }
        break;
    }
    case QMetaType::QPixmap: {
        const QPixmap p = value.value<QPixmap>();
        if (p.isNull()) {
            break;
        }

        QPixmap deco(16, 16);
        QPainter painter(&deco);
        Util::drawTransparencyPattern(&painter, deco.rect(), 4);

        QPixmap scaled(p);
        if (p.width() > deco.width() || p.height() > deco.height()) {
            scaled = p.scaled(deco.width(), deco.height(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        painter.drawPixmap((deco.width() - scaled.width()) / 2, (deco.height() - scaled.height()) / 2, scaled);

        painter.drawRect(0, 0, deco.width() - 1, deco.height() - 1);
        return deco;
    }
    default:
        break;
    }

    return QVariant();
}

void VariantHandler::registerStringConverter(int type, Converter<QString> *converter)
{
    Q_ASSERT(!s_variantHandlerRepository()->stringConverters.contains(type));
    s_variantHandlerRepository()->stringConverters.insert(type, converter);
}

void VariantHandler::registerGenericStringConverter(
    VariantHandler::GenericStringConverter converter)
{
    s_variantHandlerRepository()->genericStringConverters.push_back(converter);
}

QVariant VariantHandler::serializableVariant(const QVariant &value)
{
    if (value.userType() == qMetaTypeId<const QMatrix4x4 *>()) {
        const QMatrix4x4 *m = value.value<const QMatrix4x4 *>();
        if (!m) {
            return QVariant();
        }
        return QVariant::fromValue(QMatrix4x4(*m));
    }
    if (EnumRepositoryServer::isEnum(value.userType())) {
        return QVariant::fromValue(EnumRepositoryServer::valueFromVariant(value));
    }

    if (isEnum(value)) {
        return QVariant(value.value<int>());
    }

    return value;
}

void VariantHandler::clear()
{
    s_variantHandlerRepository()->clear();
}
