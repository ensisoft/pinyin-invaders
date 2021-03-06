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

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <memory>
#include <list>
#include <chrono>

#include <boost/lockfree/queue.hpp>

namespace audio
{
    class Device;
    class Stream;
    class Source;

    // Play audio samples using the given audio device. Once audio 
    // is played the results are stored in TrackEvents which can be
    // retrieved by a call to GetEvent. The application should 
    // periodically call this function and remove the pending
    // track events and do any processing (such as starting the next
    // audio track) it wishes to do.
    class Player
    {
    public:
        // Track specific playback status.
        enum class TrackStatus {
            // track was played successfully.
            Success,
            // track failed to play
            Failure
        };
        // Historical event/record of some sample playback.
        // you can get this data through a call to get_event
        struct TrackEvent {
            // the id of the track that was played.
            std::size_t id = 0;
            // the time point when the track was played.
            std::chrono::steady_clock::time_point when;
            // what was the result
            TrackStatus status = TrackStatus::Success;
            // whether set to looping or not
            bool looping = false;
        };

        // Create a new audio player using the given audio device.
        Player(std::unique_ptr<Device> device);
        // dtor.
       ~Player();

        // Play the audio samples sourced from the source object after some delay
        // i.e. the given time elapses (i.e. in X milliseconds after now).
        // Returns an identifier for the audio play back that will happen later.
        // The same id can be used in a call to pause/resume.
        std::size_t Play(std::unique_ptr<Source> source, std::chrono::milliseconds ms, bool looping = false);

        // Play the audio samples sourced from the source object starting immediately.
        // Returns an identifier for the audio play back that will happen later.
        // The same id can be used in a call to pause/resume.
        std::size_t Play(std::unique_ptr<Source> source, bool looping = false);

        // Pause the audio stream identified by id. 
        void Pause(std::size_t id);

        // Resume the audio stream identified by id.
        void Resume(std::size_t id);

        // Cancel (stop playback and delete the rest of the stream) of the given audio stream.
        void Cancel(std::size_t id);

        // Get next historical track event.
        // Returns true if there was a track event otherwise false.
        bool GetEvent(TrackEvent* event);

    private:
        void AudioThreadLoop(Device* ptr);

    private:
        struct Track {
            std::size_t id = 0;
            std::unique_ptr<Source> source;
            std::shared_ptr<Stream> stream;
            std::chrono::steady_clock::time_point when;
            bool looping = false;

            bool operator<(const Track& other) const {
                return when < other.when;
            }
            bool operator>(const Track& other) const {
                return when > other.when;
            }
        };
        struct Action {
            enum class Type {
                None, Resume, Pause,  Cancel
            };
            Type do_what = Type::None;
            std::size_t track_id = 0;
        };
    private:
        std::unique_ptr<std::thread> thread_;
        std::mutex queue_mutex_;
        std::mutex event_mutex_;

        // queue of actions for tracks (pause/resume)
        boost::lockfree::queue<Action> track_actions_;
        
        // unique track id
        std::size_t trackid_ = 1;

        // currently enqueued and waiting tracks.
        using audio_pq = std::priority_queue<Track, std::vector<Track>,
            std::greater<Track>>;
        audio_pq waiting_;

        // currently playing tracks
        std::list<Track> playing_;

        // list of track completion events of tracks
        // that were played.
        std::queue<TrackEvent> events_;

        // audio thread stop flag
        std::atomic_flag run_thread_ = ATOMIC_FLAG_INIT;
    };


} // namespace
