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
#  include "ui_particlewidget.h"
#  include <QStringList>
#  include <QString>
#include "warnpop.h"

#include <memory>
#include <vector>

#include "graphics/painter.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "mainwidget.h"

namespace app {
    class Resource;
    class Workspace;
} // app

namespace gui
{
    class ParticleEditorWidget : public MainWidget
    {
        Q_OBJECT
    public:
        ParticleEditorWidget(app::Workspace* workspace);
        ParticleEditorWidget(app::Workspace* workspace, const app::Resource& resource);
       ~ParticleEditorWidget();

        virtual void addActions(QToolBar& bar) override;
        virtual void addActions(QMenu& menu) override;
        virtual bool saveState(Settings& settings) const;
        virtual bool loadState(const Settings& settings);
        virtual void reloadShaders() override;
        virtual void reloadTextures() override;
        virtual void shutdown() override;
        virtual void animate(double secs) override;
        virtual void setTargetFps(unsigned fps) override;

    private slots:
        void on_actionPlay_triggered();
        void on_actionStop_triggered();
        void on_actionPause_triggered();
        void on_actionSave_triggered();
        void on_resetTransform_clicked();
        void on_plus90_clicked();
        void on_minus90_clicked();
        void paintScene(gfx::Painter& painter, double secs);
        void newResourceAvailable(const app::Resource* resource);
        void resourceToBeDeleted(const app::Resource* resource);

    private:
        void fillParams(gfx::KinematicsParticleEngine::Params& params) const;

    private:
        Ui::ParticleWidget mUI;
    private:
        app::Workspace* mWorkspace = nullptr;
        std::unique_ptr<gfx::KinematicsParticleEngine> mEngine;
        std::shared_ptr<gfx::Material> mMaterial;
        bool mPaused = false;
        float mTime  = 0.0f;
    private:
        // cache the name of the current material so that we can
        // compare and detect if the user has selected different material
        // from the material combo and then recreate the material instance.
        QString mMaterialName;
    };

} // namespace
