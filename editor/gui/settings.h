// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <QSettings>
#  include <QVariant>
#  include <QString>
#  include <QtWidgets>
#  include <QSignalBlocker>

#  include <color_selector.hpp> // from Qt color widgets 
#include "warnpop.h"

namespace gui
{
    // Settings wrapper
    class Settings
    {
    public:
        Settings(const QString& organization,
                 const QString& application)
            : mSettings(organization, application)
        {}

        // Get a value from the settings object under the specific key 
        // under a specific module. If the module/key pair doesn't exist
        // then the default value is returned.
        template<typename T>
        T getValue(const QString& module, const QString& key, const T& defaultValue) const
        {
            const auto& value = mSettings.value(module + "/" + key);
            if (!value.isValid())
                return defaultValue;
            return qvariant_cast<T>(value);
        }

        // Set a value in the settings object under the specific key
        // under a specific module. 
        template<typename T>
        void setValue(const QString& module, const QString& key, const T& value)
        {
            mSettings.setValue(module + "/" + key, value);
        }

        // Save the UI state of a widget.
        void saveWidget(const QString& module, const QTableView* table)
        {
            const auto* model  = table->model();
            const auto objName = table->objectName();
            const auto numCols = model->columnCount();

            for (int i=0; i<numCols-1; ++i)
            {
                const auto& key = QString("%1_column_%2").arg(objName).arg(i);
                const auto width = table->columnWidth(i);
                setValue(module, key, width);
            }
            if (table->isSortingEnabled())
            {
                const QHeaderView* header = table->horizontalHeader();
                const auto sortColumn = header->sortIndicatorSection();
                const auto sortOrder  = header->sortIndicatorOrder();
                setValue(module, objName + "/sort_column", sortColumn);
                setValue(module, objName + "/sort_order", sortOrder);
            }
        }
        void saveWidget(const QString& module, const color_widgets::ColorSelector* color)
        {
            setValue(module, color->objectName(), color->color());
        }
        void saveWidget(const QString& module, const QComboBox* cmb)
        {
            setValue(module, cmb->objectName(), cmb->currentText());
        }
        void saveWidget(const QString& module, const QLineEdit* line)
        {
            setValue(module, line->objectName(), line->text());
        }
        void saveWidget(const QString& module, const QSpinBox* spin) 
        {
            setValue(module, spin->objectName(), spin->value());
        }
        void saveWidget(const QString& module, const QDoubleSpinBox* spin)
        {
            setValue(module, spin->objectName(), spin->value());
        }
        void saveWidget(const QString& module, const QGroupBox* grp)
        {   
            setValue(module, grp->objectName(), grp->isChecked());
        }
        // Save the UI state of a widget.
        void saveWidget(const QString& module, const QCheckBox* chk)
        {
            setValue(module, chk->objectName(), chk->isChecked());
        }
        // Save the UI state of a widget.
        void saveWidget(const QString& module, const QSplitter* splitter)
        {
            setValue(module, splitter->objectName(), splitter->saveState());
        }
        // Load the UI state of a widget.
        void loadWidget(const QString& module, QSplitter* splitter) const
        {
            splitter->restoreState(getValue(module, splitter->objectName(), splitter->saveState()));
        }
        // Load the UI state of a widget.
        void loadWidget(const QString& module, QTableView* table) const
        {
            const auto* model  = table->model();
            const auto objName = table->objectName();
            const auto numCols = model->columnCount();

            QSignalBlocker s(table);

            for (int i=0; i<numCols-1; ++i)
            {
                const auto& key  = QString("%1_column_%2").arg(objName).arg(i);
                const auto width = getValue(module, key, table->columnWidth(i));
                table->setColumnWidth(i, width);
            }
            if (table->isSortingEnabled())
            {
                const QHeaderView* header = table->horizontalHeader();
                const auto column = getValue(module, objName + "/sort_column",
                    (int)header->sortIndicatorSection());
                const auto order = getValue(module, objName + "/sort_order",
                    (int)header->sortIndicatorOrder());
                table->sortByColumn(column,(Qt::SortOrder)order);
            }
        }
        // Load the UI state of a widget.
        void loadWidget(const QString& module, QComboBox* cmb) const 
        {   
            const auto& text = getValue(module, cmb->objectName(), 
                cmb->currentText());
            const auto index = cmb->findText(text);
            if (index != -1)
                cmb->setCurrentIndex(index);
        }
        void loadWidget(const QString& module, QCheckBox* chk) const
        {
            const bool value = getValue<bool>(module, chk->objectName(), 
                chk->isChecked());
            QSignalBlocker s(chk);
            chk->setChecked(value);
        }
        void loadWidget(const QString& module, QGroupBox* grp) const 
        {
            const bool value = getValue<bool>(module, grp->objectName(), 
                grp->isChecked());
            QSignalBlocker s(grp);
            grp->setChecked(value);
        }
        void loadWidget(const QString& module, QDoubleSpinBox* spin) const 
        {
            const double value = getValue<double>(module, spin->objectName(), 
                spin->value());
            QSignalBlocker s(spin);
            spin->setValue(value);
        }
        void loadWidget(const QString& module, QSpinBox* spin) const 
        {
            const int value = getValue<int>(module, spin->objectName(), 
                spin->value());
            QSignalBlocker s(spin);
            spin->setValue(value);
        }
        void loadWidget(const QString& module, QLineEdit* line) const 
        {
            const auto& str = getValue<QString>(module, line->objectName(), 
                line->text());
            QSignalBlocker s(line);
            line->setText(str);
        }
        void loadWidget(const QString& module, color_widgets::ColorSelector* selector) const 
        {
            const auto& color = getValue<QColor>(module, selector->objectName(), 
                selector->color());
            QSignalBlocker s(selector);
            selector->setColor(color);
        }
    private:
        QSettings mSettings;
    };
} // namespace