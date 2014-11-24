// Copyright (c) 2010-2014 Sami Väisänen, Ensisoft 
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
#  include <QtGui/QWidget>
#  include <QObject>
#  include <QTimer>
#  include <QElapsedTimer>
#include "warnpop.h"

#include <list>
#include <map>
#include <memory>
#include <chrono>

namespace invaders
{
    class Game;

    class GameWidget : public QWidget
    {
        Q_OBJECT

    public:
        GameWidget(QWidget* parent);
       ~GameWidget();

        void startGame();

    private:
        virtual void timerEvent(QTimerEvent* timer) override;
        virtual void paintEvent(QPaintEvent* paint) override;
        virtual void keyPressEvent(QKeyEvent* press) override;
    private:
        class Entity;
        class Background;
        class Display;
        class Starfield;
        class Player;
        class Debug;

    private:
        std::unique_ptr<Background> background_;
        std::unique_ptr<Display> display_;
        std::unique_ptr<Debug> debug_;
        std::unique_ptr<Player> player_;
        std::list<std::unique_ptr<Entity>> entities_;
        std::unique_ptr<Game> game_;
    };

} // invaders