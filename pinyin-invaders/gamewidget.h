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

#include <list>
#include <map>
#include <memory>
#include <stack>
#include <string>

namespace gfx {
    class Painter;
    class Device;
} // namespace
namespace wdk {
    class Window;
    struct WindowEventKeydown;
} // namespace wdk
namespace audio {
    class AudioSample;
} // namespace audio

namespace invaders
{
    class Game;
    class Level;

    class GameWidget
    {
    public:
        // Level info for persisting level data
        struct LevelInfo {
            std::wstring name;
            unsigned highScore = 0;
            bool locked = true;
        };

        // game profile settings, for example "easy", "medium" etc.
        struct Profile {
            std::wstring name;
            float speed = 0.0f; 
            unsigned spawnCount = 0;
            unsigned spawnInterval = 0;
            unsigned numEnemies = 0;
        };

        GameWidget();
       ~GameWidget();

        // load level data from the specified data file
        void loadLevels(const std::wstring& file);

        // unlock the level identified by it's name
        void unlockLevel(const std::wstring& name);

        // restore previously stored level info
        void setLevelInfo(const LevelInfo& info);

        // retrieve the lavel info for the level at index.
        // returns true when index is a valid index, otherwise false.
        bool getLevelInfo(LevelInfo& info, unsigned index) const;

        // set the gaming profile
        void setProfile(const Profile& profile);

        // launch the game contents.
        void launchGame();

        // step the game forward by dt milliseconds.
        void updateGame(float dt);

        // render current game state.
        void renderGame();

        // initialize the OpenGL rendering surface (window)
        // based on the previous size (if any). 
        void initializeGL(unsigned prev_surface_width, 
            unsigned prev_surface_height);

        // Set window to fullscreen if value is true.
        void setFullscreen(bool value);

        // Returns true if in fullscreen mode, otherwise false.
        bool isFullscreen() const;

        // close the window and dispose of the rendering context.
        void close();

        // pump window messages.
        void pumpMessages();

        // set to true to unlock all levels.
        void setMasterUnlock(bool onOff);

        // get current rendering surface (window) width.
        unsigned getSurfaceWidth() const;

        // get current rendering surface (window) height.
        unsigned getSurfaceHeight() const;

        // set to true to have unlimited time warps
        void setUnlimitedWarps(bool onOff)
        { mUnlimitedWarps = onOff; }

        // set to true to have unlimited bombs
        void setUnlimitedBombs(bool onOff)
        { mUnlimitedBombs = onOff; }

        // set to true to play sound effects.
        void setPlaySounds(bool onOff);

        // set to true to play awesome game music
        void setPlayMusic(bool onOff)
        { mPlayMusic = onOff; }

        // set to true to display current fps
        void setShowFps(bool onOff)
        { mShowFps = onOff; }

        // set most current fps.
        // if setShowFps has been set to true will display
        // the current fps in the top left corner of the window.
        void setFps(float fps)
        { mCurrentfps = fps; }

        bool getPlaySounds() const
        { return mPlaySounds; }

        bool getPlayMusic() const
        { return mPlayMusic; }

        bool isRunning() const
        { return mRunning; }
    
    private:
        void playMusic();
        void onKeyDown(const wdk::WindowEventKeydown& key);

    private:
        class State;
        class MainMenu;
        class GameHelp;
        class Settings;
        class About;
        class PlayGame;
        class Scoreboard;

        class Animation;
        class Asteroid;
        class Explosion;
        class Smoke;
        class Sparks;
        class Debris;
        class BigExplosion;
        class Score;
        class Invader;
        class Missile;
        class UFO;
        class Background;

    private:
        std::unique_ptr<Background> mBackground;
        std::stack<std::unique_ptr<State>> mStates;

        std::map<unsigned, std::unique_ptr<Invader>> mInvaders;
        std::vector<std::unique_ptr<Level>> mLevels;
        std::vector<LevelInfo> mLevelInfos;
        std::vector<Profile> mProfiles;
        std::list<std::unique_ptr<Animation>> mAnimations;

        std::unique_ptr<Game> mGame;
        unsigned mCurrentLevel   = 0;
        unsigned mCurrentProfile = 0;
        float mTickDelta     = 0.0f;
        float mWarpRemaining = 0.0f;
        float mWarpFactor    = 1.0f;
        float mCurrentfps    = 0.0f;
    private:
        bool mMasterUnlock   = false;
        bool mUnlimitedBombs = false;
        bool mUnlimitedWarps = false;
        bool mPlaySounds = true;
        bool mPlayMusic  = true;
        bool mShowFps    = false;
        bool mRunning    = true;
    private:
#ifdef GAME_ENABLE_AUDIO
        std::vector<std::shared_ptr<audio::AudioSample>> mMusicTracks;
        std::size_t mMusicTrackId = 0;
        std::size_t mMusicTrackIndex = 0;
#endif
    private:
        std::shared_ptr<gfx::Device> mCustomGraphicsDevice;
        std::unique_ptr<gfx::Painter> mCustomGraphicsPainter;
        std::unique_ptr<wdk::Window> mWindow;  
    };

} // invaders
