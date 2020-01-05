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
#include "warnpop.h"

#include <memory>

#include "graphics/painter.h"
#include "graphics/drawable.h"
#include "mainwidget.h"

namespace gui 
{
    class ParticleEditorWidget : public MainWidget
    {
        Q_OBJECT
    public:
        ParticleEditorWidget();
       ~ParticleEditorWidget();

        virtual void addActions(QToolBar& bar) override;
        virtual void addActions(QMenu& menu) override;       
        virtual bool saveState(Settings& settings) const;
        virtual bool loadState(const Settings& settings);

    private slots:
        void on_actionPlay_triggered();
        void on_actionStop_triggered();
        void on_actionPause_triggered();
        void on_browseTexture_clicked();
        void on_resetTransform_clicked();
        void on_plus90_clicked();
        void on_minus90_clicked();
        void paintScene(gfx::Painter& painter, double secs);
        
    private:
        Ui::ParticleWidget mUI;
    private:
        std::unique_ptr<gfx::KinematicsParticleEngine> mEngine;
        bool mPaused = false;
        float mTime  = 0.0f;
    private:
        QStringList mTextures;
    };

} // namespace