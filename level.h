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
#  include <QString>
#include "warnpop.h"

#include <cstddef>
#include <vector>
#include <memory>

namespace invaders
{
    // Level is a factory for spawning enemies and configuring the game 
    class Level 
    {
    public:
        struct enemy {

            // the points you get for killing this bastard
            unsigned score;

            // enemy representation string.
            QString  string;

            // the correct killstring to kill the enemy
            QString  killstring;

            // help/description of the enemy 
            // (word definition)
            QString  help;
        };

        Level();
       ~Level();

        // load data from the a single file
        void load(const QString& file);

        void reset();

        // spawn a new enemy
        enemy spawn();

        void unlock() 
        { locked_ = false; }

        // return the number of enemies to spawn per one spawn
        unsigned spawnCount() const
        { return spawncount_; }

        // return the total number of enemies this level has
        unsigned enemyCount() const 
        { return enemycount_; }

        // return the interval between spawning new enemies
        // the interval is expressed in game ticks
        unsigned spawnInterval() const 
        { return spawninterval_; }

        QString description() const 
        { return description_; }

        // get a list of available enemies in this level.
        const std::vector<enemy>& getEnemies() const
        { return enemies_; }

        bool isLocked() const 
        { return locked_; }


        // load a list of levels from a level file.
        static std::vector<std::unique_ptr<Level>> loadLevels(const QString& file);

    private:
        unsigned spawncount_;
        unsigned spawninterval_;
        unsigned enemycount_;
        QString  description_;
        bool locked_;
    private:
        std::vector<enemy> enemies_;
        std::size_t max_;
    };
} // invaders