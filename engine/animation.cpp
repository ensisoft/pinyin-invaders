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

#include "config.h"

#include "warnpush.h"
#  include <nlohmann/json.hpp>
#  include <neargye/magic_enum.hpp>
#include "warnpop.h"

#include <cmath>

#include "base/logging.h"
#include "base/assert.h"
#include "base/utility.h"
#include "engine/animation.h"
#include "engine/entity.h"

namespace game
{

std::size_t SetFlagActuatorClass::GetHash() const
{
    std::size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mNodeId);
    hash = base::hash_combine(hash, mFlagName);
    hash = base::hash_combine(hash, mStartTime);
    hash = base::hash_combine(hash, mDuration);
    hash = base::hash_combine(hash, mFlagAction);
    return hash;
}

nlohmann::json SetFlagActuatorClass::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "id",        mId);
    base::JsonWrite(json, "node",      mNodeId);
    base::JsonWrite(json, "flag",      mFlagName);
    base::JsonWrite(json, "starttime", mStartTime);
    base::JsonWrite(json, "duration",  mDuration);
    base::JsonWrite(json, "action",    mFlagAction);
    return json;
}

bool SetFlagActuatorClass::FromJson(const nlohmann::json& json)
{
    return base::JsonReadSafe(json, "id",        &mId) &&
           base::JsonReadSafe(json, "node",      &mNodeId) &&
           base::JsonReadSafe(json, "flag",      &mFlagName) &&
           base::JsonReadSafe(json, "starttime", &mStartTime) &&
           base::JsonReadSafe(json, "duration",  &mDuration) &&
           base::JsonReadSafe(json, "action",    &mFlagAction);
}

std::size_t KinematicActuatorClass::GetHash() const
{
    std::size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mNodeId);
    hash = base::hash_combine(hash, mInterpolation);
    hash = base::hash_combine(hash, mStartTime);
    hash = base::hash_combine(hash, mDuration);
    hash = base::hash_combine(hash, mEndLinearVelocity);
    hash = base::hash_combine(hash, mEndAngularVelocity);
    return hash;
}

nlohmann::json KinematicActuatorClass::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "id",               mId);
    base::JsonWrite(json, "node",             mNodeId);
    base::JsonWrite(json, "method",           mInterpolation);
    base::JsonWrite(json, "starttime",        mStartTime);
    base::JsonWrite(json, "duration",         mDuration);
    base::JsonWrite(json, "linear_velocity",  mEndLinearVelocity);
    base::JsonWrite(json, "angular_velocity", mEndAngularVelocity);
    return json;
}

bool KinematicActuatorClass::FromJson(const nlohmann::json &json)
{
    return base::JsonReadSafe(json, "id",               &mId) &&
           base::JsonReadSafe(json, "node",             &mNodeId) &&
           base::JsonReadSafe(json, "method",           &mInterpolation) &&
           base::JsonReadSafe(json, "starttime",        &mStartTime) &&
           base::JsonReadSafe(json, "duration",         &mDuration) &&
           base::JsonReadSafe(json, "linear_velocity",  &mEndLinearVelocity) &&
           base::JsonReadSafe(json, "angular_velocity", &mEndAngularVelocity);
}

size_t SetValueActuatorClass::GetHash() const
{
    std::size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mNodeId);
    hash = base::hash_combine(hash, mInterpolation);
    hash = base::hash_combine(hash, mParamName);
    hash = base::hash_combine(hash, mStartTime);
    hash = base::hash_combine(hash, mDuration);
    hash = base::hash_combine(hash, mEndValue);
    return hash;
}

nlohmann::json SetValueActuatorClass::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "id",        mId);
    base::JsonWrite(json, "node",      mNodeId);
    base::JsonWrite(json, "method",    mInterpolation);
    base::JsonWrite(json, "name",      mParamName);
    base::JsonWrite(json, "starttime", mStartTime);
    base::JsonWrite(json, "duration",  mDuration);
    base::JsonWrite(json, "value",     mEndValue);
    return json;
}

bool SetValueActuatorClass::FromJson(const nlohmann::json &json)
{
    return base::JsonReadSafe(json, "id",        &mId) &&
           base::JsonReadSafe(json, "node",      &mNodeId) &&
           base::JsonReadSafe(json, "method",    &mInterpolation) &&
           base::JsonReadSafe(json, "name",      &mParamName) &&
           base::JsonReadSafe(json, "starttime", &mStartTime) &&
           base::JsonReadSafe(json, "duration",  &mDuration) &&
           base::JsonReadSafe(json, "value",     &mEndValue);
}

nlohmann::json TransformActuatorClass::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "id",        mId);
    base::JsonWrite(json, "node",      mNodeId);
    base::JsonWrite(json, "method",    mInterpolation);
    base::JsonWrite(json, "starttime", mStartTime);
    base::JsonWrite(json, "duration",  mDuration);
    base::JsonWrite(json, "position",  mEndPosition);
    base::JsonWrite(json, "size",      mEndSize);
    base::JsonWrite(json, "scale",     mEndScale);
    base::JsonWrite(json, "rotation",  mEndRotation);
    return json;
}

bool TransformActuatorClass::FromJson(const nlohmann::json& json)
{
    return base::JsonReadSafe(json, "id",        &mId) &&
           base::JsonReadSafe(json, "node",      &mNodeId) &&
           base::JsonReadSafe(json, "starttime", &mStartTime) &&
           base::JsonReadSafe(json, "duration",  &mDuration) &&
           base::JsonReadSafe(json, "position",  &mEndPosition) &&
           base::JsonReadSafe(json, "size",      &mEndSize) &&
           base::JsonReadSafe(json, "scale",     &mEndScale) &&
           base::JsonReadSafe(json, "rotation",  &mEndRotation) &&
           base::JsonReadSafe(json, "method",    &mInterpolation);
}

std::size_t TransformActuatorClass::GetHash() const
{
    std::size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mNodeId);
    hash = base::hash_combine(hash, mInterpolation);
    hash = base::hash_combine(hash, mStartTime);
    hash = base::hash_combine(hash, mDuration);
    hash = base::hash_combine(hash, mEndPosition);
    hash = base::hash_combine(hash, mEndSize);
    hash = base::hash_combine(hash, mEndScale);
    hash = base::hash_combine(hash, mEndRotation);
    return hash;
}

void KinematicActuator::Start(EntityNode& node)
{
    if (const auto* body = node.GetRigidBody())
    {
        mStartLinearVelocity  = body->GetLinearVelocity();
        mStartAngularVelocity = body->GetAngularVelocity();
        if (body->GetSimulation() != RigidBodyItemClass::Simulation::Kinematic)
        {
            WARN("EntityNode '%1' is not kinematically simulated.", node.GetName());
            WARN("Kinematic actuator will have no effect.");
        }
    }
    else
    {
        WARN("EntityNode '%1' doesn't have a rigid body item.", node.GetName());
        WARN("Kinematic actuator will have no effect.");
    }
}
void KinematicActuator::Apply(EntityNode& node, float t)
{
    if (auto* body = node.GetRigidBody())
    {
        const auto method = mClass->GetInterpolation();
        const auto linear_velocity = math::interpolate(mStartLinearVelocity, mClass->GetEndLinearVelocity(), t, method);
        const auto angular_velocity = math::interpolate(mStartAngularVelocity, mClass->GetEndAngularVelocity(), t, method);
        body->SetLinearVelocity(linear_velocity);
        body->SetAngularVelocity(angular_velocity);
    }
}

void KinematicActuator::Finish(EntityNode& node)
{
    if (auto* body = node.GetRigidBody())
    {
        body->SetLinearVelocity(mClass->GetEndLinearVelocity());
        body->SetAngularVelocity(mClass->GetEndAngularVelocity());
    }
}

void SetFlagActuator::Start(EntityNode& node)
{
    if (const auto* item = node.GetDrawable())
    {
        const auto flag = magic_enum::enum_cast<DrawableItemClass::Flags>(mClass->GetFlagName());
        if (flag.has_value())
        {
            mStartState = item->TestFlag(flag.value());
            return;
        }
    }
    if (const auto* item = node.GetRigidBody())
    {
        const auto flag = magic_enum::enum_cast<RigidBodyItemClass::Flags>(mClass->GetFlagName());
        if (flag.has_value())
        {
            mStartState = item->TestFlag(flag.value());
            return;
        }
    }
    WARN("Unidentified node flag '%1'", mClass->GetFlagName());
}
void SetFlagActuator::Apply(EntityNode& node, float t)
{
    // no op.
}
void SetFlagActuator::Finish(EntityNode& node)
{
    bool next_value = false;
    const auto action = mClass->GetFlagAction();
    if (action == FlagAction::Toggle)
        next_value = !mStartState;
    else if (action == FlagAction::On)
        next_value = true;
    else if (action == FlagAction::Off)
        next_value = false;

    if (auto* item = node.GetDrawable())
    {
        const auto flag = magic_enum::enum_cast<DrawableItemClass::Flags>(mClass->GetFlagName());
        if (flag.has_value())
        {
            item->SetFlag(flag.value(), next_value);
            return;
        }
    }
    if (auto* item = node.GetRigidBody())
    {
        const auto flag = magic_enum::enum_cast<RigidBodyItemClass::Flags>(mClass->GetFlagName());
        if (flag.has_value())
        {
            item->SetFlag(flag.value(), next_value);
            return;
        }
    }
    WARN("Unidentified node flag '%1'", mClass->GetFlagName());
}

void SetValueActuator::Start(EntityNode& node)
{
    const auto param  = mClass->GetParamName();
    const auto* draw  = node.GetDrawable();
    const auto* body  = node.GetRigidBody();
    if (param == ParamName::AlphaOverride || param == ParamName::DrawableTimeScale)
    {
        if (!draw)
        {
            WARN("EntityNode '%1' doesn't have a drawable item." , node.GetName());
            WARN("Setting '%1' will have no effect.", param);
            return;
        }
    }
    else if (param == ParamName::LinearVelocityY || param == ParamName::LinearVelocityX ||
             param == ParamName::AngularVelocity)
    {
        if (!body)
        {
            WARN("EntityNode '%1' doesn't have a rigid body ." , node.GetName());
            WARN("Setting '%1' will have no effect.", param);
            return;
        }
    }

    if (param == ParamName::AlphaOverride)
    {
        if (!draw->TestFlag(DrawableItemClass::Flags::OverrideAlpha))
        {
            WARN("EntityNode '%1' doesn't set OverrideAlpha flag. " , node.GetName());
            WARN("Setting '%1' value will have no effect.", param);
        }
        mStartValue = draw->GetAlpha();
    }
    else if (param == ParamName::DrawableTimeScale)
        mStartValue = draw->GetTimeScale();
    else if (param == ParamName::AngularVelocity)
        mStartValue = body->GetAngularVelocity();
    else if (param == ParamName::LinearVelocityX)
        mStartValue = body->GetLinearVelocity().x;
    else if (param == ParamName::LinearVelocityY)
        mStartValue = body->GetLinearVelocity().y;

}
void SetValueActuator::Apply(EntityNode& node, float t)
{
    const auto method = mClass->GetInterpolation();
    const auto param  = mClass->GetParamName();
    const float value = math::interpolate(mStartValue, mClass->GetEndValue(), t, method);
    auto* draw = node.GetDrawable();
    auto* body = node.GetRigidBody();

    if (param == ParamName::AlphaOverride && draw)
        draw->SetAlpha(value);
    else if (param == ParamName::DrawableTimeScale && draw)
        draw->SetTimeScale(value);
    else if (param == ParamName::AngularVelocity && body)
        body->SetAngularVelocity(value);
    else if (param == ParamName::LinearVelocityX && body)
    {
        auto velocity = body->GetLinearVelocity();
        velocity.x = value;
        body->SetLinearVelocity(velocity);
    }
    else if (param == ParamName::LinearVelocityY && body)
    {
        auto velocity = body->GetLinearVelocity();
        velocity.y = value;
        body->SetLinearVelocity(velocity);
    }
}

void SetValueActuator::Finish(EntityNode& node)
{
    if (auto* draw = node.GetDrawable())
    {
        draw->SetAlpha(mClass->GetEndValue());
    }
}

void TransformActuator::Start(EntityNode& node)
{
    mStartPosition = node.GetTranslation();
    mStartSize     = node.GetSize();
    mStartScale    = node.GetScale();
    mStartRotation = node.GetRotation();
}
void TransformActuator::Apply(EntityNode& node, float t)
{
    // apply interpolated state on the node.
    const auto method = mClass->GetInterpolation();
    const auto& p = math::interpolate(mStartPosition, mClass->GetEndPosition(), t, method);
    const auto& s = math::interpolate(mStartSize,     mClass->GetEndSize(),     t, method);
    const auto& r = math::interpolate(mStartRotation, mClass->GetEndRotation(), t, method);
    const auto& f = math::interpolate(mStartScale,    mClass->GetEndScale(),    t, method);
    node.SetTranslation(p);
    node.SetSize(s);
    node.SetRotation(r);
    node.SetScale(f);
}
void TransformActuator::Finish(EntityNode& node)
{
    node.SetTranslation(mClass->GetEndPosition());
    node.SetRotation(mClass->GetEndRotation());
    node.SetSize(mClass->GetEndSize());
    node.SetScale(mClass->GetEndScale());
}

AnimationTrackClass::AnimationTrackClass(const AnimationTrackClass& other)
{
    for (const auto& a : other.mActuators)
    {
        mActuators.push_back(a->Copy());
    }
    mId       = other.mId;
    mName     = other.mName;
    mDuration = other.mDuration;
    mLooping  = other.mLooping;
    mDelay    = other.mDelay;
}
AnimationTrackClass::AnimationTrackClass(AnimationTrackClass&& other)
{
    mId        = std::move(other.mId);
    mActuators = std::move(other.mActuators);
    mName      = std::move(other.mName);
    mDuration  = other.mDuration;
    mLooping   = other.mLooping;
    mDelay     = other.mDelay;
}

void AnimationTrackClass::DeleteActuator(size_t index)
{
    ASSERT(index < mActuators.size());
    auto it = mActuators.begin();
    std::advance(it, index);
    mActuators.erase(it);
}

bool AnimationTrackClass::DeleteActuatorById(const std::string& id)
{
    for (auto it = mActuators.begin(); it != mActuators.end(); ++it)
    {
        if ((*it)->GetId() == id) {
            mActuators.erase(it);
            return true;
        }
    }
    return false;
}
ActuatorClass* AnimationTrackClass::FindActuatorById(const std::string& id)
{
    for (auto& actuator : mActuators) {
        if (actuator->GetId() == id)
            return actuator.get();
    }
    return nullptr;
}
const ActuatorClass* AnimationTrackClass::FindActuatorById(const std::string& id) const
{
    for (auto& actuator : mActuators) {
        if (actuator->GetId() == id)
            return actuator.get();
    }
    return nullptr;
}

std::unique_ptr<Actuator> AnimationTrackClass::CreateActuatorInstance(size_t i) const
{
    const auto& klass = mActuators[i];
    if (klass->GetType() == ActuatorClass::Type::Transform)
        return std::make_unique<TransformActuator>(std::static_pointer_cast<TransformActuatorClass>(klass));
    else if (klass->GetType() == ActuatorClass::Type::SetValue)
        return std::make_unique<SetValueActuator>(std::static_pointer_cast<SetValueActuatorClass>(klass));
    else if (klass->GetType() == ActuatorClass::Type::Kinematic)
        return std::make_unique<KinematicActuator>(std::static_pointer_cast<KinematicActuatorClass>(klass));
    else if (klass->GetType() == ActuatorClass::Type::SetFlag)
        return std::make_unique<SetFlagActuator>(std::static_pointer_cast<SetFlagActuatorClass>(klass));
    else BUG("Unknown actuator type");
    return {};
}

std::size_t AnimationTrackClass::GetHash() const
{
    std::size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mDuration);
    hash = base::hash_combine(hash, mLooping);
    hash = base::hash_combine(hash, mDelay);
    for (const auto& actuator : mActuators)
        hash = base::hash_combine(hash, actuator->GetHash());
    return hash;
}

nlohmann::json AnimationTrackClass::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "id", mId);
    base::JsonWrite(json, "name", mName);
    base::JsonWrite(json, "duration", mDuration);
    base::JsonWrite(json, "delay", mDelay);
    base::JsonWrite(json, "looping", mLooping);
    for (const auto &actuator : mActuators)
    {
        nlohmann::json js;
        base::JsonWrite(js, "type", actuator->GetType());
        base::JsonWrite(js, "actuator", *actuator);
        json["actuators"].push_back(std::move(js));
    }
    return json;
}

// static
std::optional<AnimationTrackClass> AnimationTrackClass::FromJson(const nlohmann::json& json)
{
    AnimationTrackClass ret;
    if (!base::JsonReadSafe(json, "id", &ret.mId) ||
        !base::JsonReadSafe(json, "name", &ret.mName) ||
        !base::JsonReadSafe(json, "duration", &ret.mDuration) ||
        !base::JsonReadSafe(json, "delay", &ret.mDelay) ||
        !base::JsonReadSafe(json, "looping", &ret.mLooping))
        return std::nullopt;
    if (!json.contains("actuators"))
        return ret;
    for (const auto& json_actuator : json["actuators"].items())
    {
        const auto& obj = json_actuator.value();
        ActuatorClass::Type type;
        if (!base::JsonReadSafe(obj, "type", &type))
            return std::nullopt;
        std::shared_ptr<ActuatorClass> actuator;
        if (type == ActuatorClass::Type::Transform)
            actuator = std::make_shared<TransformActuatorClass>();
        else if (type == ActuatorClass::Type::SetValue)
            actuator = std::make_shared<SetValueActuatorClass>();
        else if (type == ActuatorClass::Type::Kinematic)
            actuator = std::make_shared<KinematicActuatorClass>();
        else if (type == ActuatorClass::Type::SetFlag)
            actuator = std::make_shared<SetFlagActuatorClass>();
        else BUG("Unknown actuator type.");

        if (!actuator->FromJson(obj["actuator"]))
            return std::nullopt;
        ret.mActuators.push_back(actuator);
    }
    return ret;
}

AnimationTrackClass AnimationTrackClass::Clone() const
{
    AnimationTrackClass ret;
    ret.mName     = mName;
    ret.mDuration = mDuration;
    ret.mLooping  = mLooping;
    ret.mDelay    = mDelay;
    for (const auto& klass : mActuators)
        ret.mActuators.push_back(klass->Clone());
    return ret;
}

AnimationTrackClass& AnimationTrackClass::operator=(const AnimationTrackClass& other)
{
    if (this == &other)
        return *this;
    AnimationTrackClass copy(other);
    std::swap(mId, copy.mId);
    std::swap(mActuators, copy.mActuators);
    std::swap(mName, copy.mName);
    std::swap(mDuration, copy.mDuration);
    std::swap(mLooping, copy.mLooping);
    std::swap(mDelay, copy.mDelay);
    return *this;
}

AnimationTrack::AnimationTrack(const std::shared_ptr<const AnimationTrackClass>& klass)
    : mClass(klass)
{
    for (size_t i=0; i<mClass->GetNumActuators(); ++i)
    {
        NodeTrack track;
        track.actuator = mClass->CreateActuatorInstance(i);
        track.node     = track.actuator->GetNodeId();
        track.ended    = false;
        track.started  = false;
        mTracks.push_back(std::move(track));
    }
    mDelay = klass->GetDelay();
    // start at negative delay time, then the actual animation playback
    // starts after the current time reaches 0 and all of the delay
    // has been "consumed".
    mCurrentTime = -mDelay;
}
AnimationTrack::AnimationTrack(const AnimationTrackClass& klass)
    : AnimationTrack(std::make_shared<AnimationTrackClass>(klass))
    {}

AnimationTrack::AnimationTrack(const AnimationTrack& other) : mClass(other.mClass)
{
    for (size_t i=0; i<other.mTracks.size(); ++i)
    {
        NodeTrack track;
        track.node     = other.mTracks[i].node;
        track.actuator = other.mTracks[i].actuator->Copy();
        track.ended    = other.mTracks[i].ended;
        track.started  = other.mTracks[i].started;
        mTracks.push_back(std::move(track));
    }
    mCurrentTime = other.mCurrentTime;
    mDelay       = other.mDelay;
}
    // Move ctor.
AnimationTrack::AnimationTrack(AnimationTrack&& other)
{
    mClass       = other.mClass;
    mCurrentTime = other.mCurrentTime;
    mDelay       = other.mDelay;
    mTracks      = std::move(other.mTracks);
}

void AnimationTrack::Update(float dt)
{
    const auto duration = mClass->GetDuration();

    mCurrentTime = math::clamp(-mDelay, duration, mCurrentTime + dt);
}

void AnimationTrack::Apply(EntityNode& node) const
{
    // if we're delaying then skip until delay is consumed.
    if (mCurrentTime < 0)
        return;
    const auto duration = mClass->GetDuration();
    const auto pos = mCurrentTime / duration;

    // todo: keep the tracks in some smarter data structure or perhaps
    // in a sorted vector and then binary search.
    for (auto& track : mTracks)
    {
        if (track.node != node.GetClassId())
            continue;

        const auto start = track.actuator->GetStartTime();
        const auto len   = track.actuator->GetDuration();
        const auto end   = math::clamp(0.0f, 1.0f, start + len);
        if (pos < start)
            continue;
        else if (pos >= end)
        {
            if (!track.ended)
            {
                track.actuator->Finish(node);
                track.ended = true;
            }
            continue;
        }
        if (!track.started)
        {
            track.actuator->Start(node);
            track.started = true;
        }
        const auto t = math::clamp(0.0f, 1.0f, (pos - start) / len);
        track.actuator->Apply(node, t);
    }
}

void AnimationTrack::Restart()
{
    for (auto& track : mTracks)
    {
        ASSERT(track.started);
        ASSERT(track.ended);
        track.started = false;
        track.ended   = false;
    }
    mCurrentTime = -mDelay;
}

bool AnimationTrack::IsComplete() const
{
    for (const auto& track : mTracks)
    {
        if (!track.ended)
            return false;
    }
    if (mCurrentTime >= mClass->GetDuration())
        return true;
    return false;
}

std::unique_ptr<AnimationTrack> CreateAnimationTrackInstance(std::shared_ptr<const AnimationTrackClass> klass)
{
    return std::make_unique<AnimationTrack>(klass);
}

} // namespace