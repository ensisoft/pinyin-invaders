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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <QString>
#include "warnpop.h"

namespace gui
{
    // Collection of global application settings.
    struct AppSettings {
        // the path to the currently set external image editor application.
        // for example /usr/bin/gimp
        QString image_editor_executable;
        // the arguments for the image editor. The special argument ${file}
        // is expanded to contain the full native absolute path to the
        // file that user wants to work with.
        QString image_editor_arguments = "${file}";
        // the path to the currently set external editor for shader (.glsl) files.
        // for example /usr/bin/gedit
        QString shader_editor_executable;
        // the arguments for the shader editor. The special argument ${file}
        // is expanded to contain the full native absolute path to the
        // file that the user wants to open.
        QString shader_editor_arguments = "${file}";
        // the path to the currently set external editor for lua script (.lua) files.
        // for example /usr/bin/gedit
        QString script_editor_executable;
        // the arguments for the shader editor. The special argument ${file}
        // is expanded to contain the full native absolute path to the
        // file that the user wants to open.
        QString script_editor_arguments = "${file}";

        // by default open resources in a new window or new tab
        QString default_open_win_or_tab = "Tab";
        // Name of the qt style currently in use.
        // kvantum is a Qt5 style engine, there's a windows port
        // available here:
        // https://github.com/ensisoft/kvantum-on-windows
        // build the style per instructions and place into dist/styles
        // note that this is a SVG rendered based style engine so the
        // QtSvg.dll also needs to be deployed.
        // kvantum is also available on linux through various package
        // managers.
        QString style_name = "kvantum";
        // Whether to save widgets with unsaved changes automatically
        // on play or whether to ask
        bool save_automatically_on_play = false;

        AppSettings()
        {
#if defined(POSIX_OS)
            image_editor_executable  = "/usr/bin/gimp";
            shader_editor_executable = "/usr/bin/gedit";
            script_editor_executable = "/usr/bin/gedit";
#elif defined(WINDOWS_OS)
            image_editor_executable  = "mspaint.exe";
            shader_editor_executable = "notepad.exe";
            script_editor_executable = "notepad.exe";
#endif
        }
    };

} // namespace
