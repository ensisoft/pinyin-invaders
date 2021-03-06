// Copyright (C) 2020-2021 Sami Väisänen
// Copyright (C) 2020-2021 Ensisoft http://www.ensisoft.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#define LOGTAG "gui"

#include "config.h"

#include "warnpush.h"
#  include <QFontDatabase>
#include "warnpop.h"

#include "editor/gui/dlgabout.h"

namespace gui
{

DlgAbout::DlgAbout(QWidget* parent) : QDialog(parent)
{
    mUI.setupUi(this);
    setWindowTitle(APP_TITLE " " APP_VERSION);

    static bool done_add_font = false;
    static int font_id = 0;
    if (!done_add_font)
    {
        font_id = QFontDatabase::addApplicationFont(":/fonts/orbitron-medium.otf");
        done_add_font = true;
    }
    QString family = QFontDatabase::applicationFontFamilies(font_id).at(0);
    QFont orbitron(family);
    orbitron.setPixelSize(30);
    mUI.title->setText(APP_TITLE " " APP_VERSION);
    mUI.title->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
    mUI.title->setFont(orbitron);
    orbitron.setPixelSize(16);
    mUI.copyright->setFont(orbitron);
    mUI.copyright->setText(APP_COPYRIGHT);
    mUI.copyright->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
    mUI.www->setFont(orbitron);
    mUI.www->setText(APP_LINKS);
    mUI.www->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
}

} // namespace
