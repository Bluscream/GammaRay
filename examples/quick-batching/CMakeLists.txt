# This file is part of GammaRay, the Qt application inspection and manipulation tool.
#
# SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Contact KDAB at <info@kdab.com> for commercial licensing options.
#

qml_lint(quick-batching.qml)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qml_lint(Slider.qml)
endif()
