
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
#  include <glm/glm.hpp> // for glm::inverse
#include "warnpop.h"

#include <algorithm>
#include <map>
#include <cmath>
#include <unordered_set>

#include "base/logging.h"
#include "base/assert.h"
#include "base/utility.h"
#include "gamelib/treeop.h"
#include "gamelib/entity.h"

namespace game
{

EntityNodeClass::EntityNodeClass(const EntityNodeClass& other)
{
    mClassId  = other.mClassId;
    mName     = other.mName;
    mPosition = other.mPosition;
    mScale    = other.mScale;
    mSize     = other.mSize;
    mRotation = other.mRotation;
    mBitFlags = other.mBitFlags;
    if (other.mRigidBody)
        mRigidBody = std::make_shared<RigidBodyItemClass>(*other.mRigidBody);
    if (other.mDrawable)
        mDrawable = std::make_shared<DrawableItemClass>(*other.mDrawable);
}

EntityNodeClass::EntityNodeClass(EntityNodeClass&& other)
{
    mClassId   = std::move(other.mClassId);
    mName      = std::move(other.mName);
    mPosition  = std::move(other.mPosition);
    mScale     = std::move(other.mScale);
    mSize      = std::move(other.mSize);
    mRotation  = std::move(other.mRotation);
    mRigidBody = std::move(other.mRigidBody);
    mDrawable  = std::move(other.mDrawable);
    mBitFlags  = std::move(other.mBitFlags);
}

std::size_t EntityNodeClass::GetHash() const
{
    std::size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mPosition);
    hash = base::hash_combine(hash, mScale);
    hash = base::hash_combine(hash, mSize);
    hash = base::hash_combine(hash, mRotation);
    hash = base::hash_combine(hash, mBitFlags.value());
    if (mRigidBody)
        hash = base::hash_combine(hash, mRigidBody->GetHash());
    if (mDrawable)
        hash = base::hash_combine(hash, mDrawable->GetHash());
    return hash;
}

void EntityNodeClass::SetRigidBody(const RigidBodyItemClass &body)
{
    mRigidBody = std::make_shared<RigidBodyItemClass>(body);
}

void EntityNodeClass::SetDrawable(const DrawableItemClass &drawable)
{
    mDrawable = std::make_shared<DrawableItemClass>(drawable);
}

glm::mat4 EntityNodeClass::GetNodeTransform() const
{
    gfx::Transform transform;
    transform.Scale(mScale);
    transform.Rotate(mRotation);
    transform.Translate(mPosition);
    return transform.GetAsMatrix();
}
glm::mat4 EntityNodeClass::GetModelTransform() const
{
    gfx::Transform transform;
    transform.Scale(mSize);
    // offset the object so that the center of the shape is aligned
    // with the position parameter.
    transform.Translate(-mSize.x * 0.5f, -mSize.y * 0.5f);
    return transform.GetAsMatrix();
}

void EntityNodeClass::Update(float time, float dt)
{
}

nlohmann::json EntityNodeClass::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "class",    mClassId);
    base::JsonWrite(json, "name",     mName);
    base::JsonWrite(json, "position", mPosition);
    base::JsonWrite(json, "scale",    mScale);
    base::JsonWrite(json, "size",     mSize);
    base::JsonWrite(json, "rotation", mRotation);
    base::JsonWrite(json, "flags", mBitFlags.value());
    if (mRigidBody)
        json["rigid_body"] = mRigidBody->ToJson();
    if (mDrawable)
        json["drawable_item"] = mDrawable->ToJson();

    return json;
}

// static
std::optional<EntityNodeClass> EntityNodeClass::FromJson(const nlohmann::json &json)
{
    unsigned flags = 0;
    EntityNodeClass ret;
    if (!base::JsonReadSafe(json, "class",    &ret.mClassId) ||
        !base::JsonReadSafe(json, "name",     &ret.mName) ||
        !base::JsonReadSafe(json, "position", &ret.mPosition) ||
        !base::JsonReadSafe(json, "scale",    &ret.mScale) ||
        !base::JsonReadSafe(json, "size",     &ret.mSize) ||
        !base::JsonReadSafe(json, "rotation", &ret.mRotation) ||
        !base::JsonReadSafe(json, "flags",    &flags))
        return std::nullopt;

    if (json.contains("rigid_body"))
    {
        auto body = RigidBodyItemClass::FromJson(json["rigid_body"]);
        if (!body.has_value())
            return std::nullopt;
        ret.mRigidBody = std::make_shared<RigidBodyItemClass>(std::move(body.value()));
    }

    if (json.contains("drawable_item"))
    {
        auto draw = DrawableItemClass::FromJson(json["drawable_item"]);
        if (!draw.has_value())
            return std::nullopt;
        ret.mDrawable = std::make_shared<DrawableItemClass>(std::move(draw.value()));
    }
    ret.mBitFlags.set_from_value(flags);
    return ret;
}

EntityNodeClass EntityNodeClass::Clone() const
{
    EntityNodeClass ret(*this);
    ret.mClassId = base::RandomString(10);
    return ret;
}

EntityNodeClass& EntityNodeClass::operator=(const EntityNodeClass& other)
{
    if (this == &other)
        return *this;
    EntityNodeClass tmp(other);
    mClassId   = std::move(tmp.mClassId);
    mName      = std::move(tmp.mName);
    mPosition  = std::move(tmp.mPosition);
    mScale     = std::move(tmp.mScale);
    mSize      = std::move(tmp.mSize);
    mRotation  = std::move(tmp.mRotation);
    mRigidBody = std::move(tmp.mRigidBody);
    mDrawable  = std::move(tmp.mDrawable);
    mBitFlags  = std::move(tmp.mBitFlags);
    return *this;
}

EntityNode::EntityNode(std::shared_ptr<const EntityNodeClass> klass)
    : mClass(klass)
{
    mInstId = base::RandomString(10);
    mName   = klass->GetName();
    Reset();
}

EntityNode::EntityNode(const EntityNode& other)
{
    mClass    = other.mClass;
    mInstId   = other.mInstId;
    mName     = other.mName;
    mScale    = other.mScale;
    mSize     = other.mSize;
    mPosition = other.mPosition;
    mRotation = other.mRotation;
    if (other.HasRigidBody())
        mRigidBody = std::make_unique<RigidBodyItem>(*other.GetRigidBody());
    if (other.HasDrawable())
        mDrawable = std::make_unique<DrawableItem>(*other.GetDrawable());
}

EntityNode::EntityNode(EntityNode&& other)
{
    mClass     = std::move(other.mClass);
    mInstId    = std::move(other.mInstId);
    mName      = std::move(other.mName);
    mScale     = std::move(other.mScale);
    mSize      = std::move(other.mSize);
    mPosition  = std::move(other.mPosition);
    mRotation  = std::move(other.mRotation);
    mRigidBody = std::move(other.mRigidBody);
    mDrawable  = std::move(other.mDrawable);
}

EntityNode::EntityNode(const EntityNodeClass& klass) : EntityNode(std::make_shared<EntityNodeClass>(klass))
{}

DrawableItem* EntityNode::GetDrawable()
{ return mDrawable.get(); }

RigidBodyItem* EntityNode::GetRigidBody()
{ return mRigidBody.get(); }

const DrawableItem* EntityNode::GetDrawable() const
{ return mDrawable.get(); }

const RigidBodyItem* EntityNode::GetRigidBody() const
{ return mRigidBody.get(); }

void EntityNode::Reset()
{
    mPosition = mClass->GetTranslation();
    mScale    = mClass->GetScale();
    mSize     = mClass->GetSize();
    mRotation = mClass->GetRotation();
    if (mClass->HasDrawable())
        mDrawable = std::make_unique<DrawableItem>(mClass->GetSharedDrawable());
    if (mClass->HasRigidBody())
        mRigidBody = std::make_unique<RigidBodyItem>(mClass->GetSharedRigidBody());
}

glm::mat4 EntityNode::GetNodeTransform() const
{
    gfx::Transform transform;
    transform.Scale(mScale);
    transform.Rotate(mRotation);
    transform.Translate(mPosition);
    return transform.GetAsMatrix();
}

glm::mat4 EntityNode::GetModelTransform() const
{
    gfx::Transform transform;
    transform.Scale(mSize);
    // offset the object so that the center of the shape is aligned
    // with the position parameter.
    transform.Translate(-mSize.x * 0.5f, -mSize.y * 0.5f);
    return transform.GetAsMatrix();
}

EntityClass::EntityClass(const EntityClass& other)
{
    mClassId = other.mClassId;

    std::unordered_map<const EntityNodeClass*, const EntityNodeClass*> map;

    // make a deep copy of the nodes.
    for (const auto& node : other.mNodes)
    {
        auto copy = std::make_unique<EntityNodeClass>(*node);
        map[node.get()] = copy.get();
        mNodes.push_back(std::move(copy));
    }

    // make a deep copy of the animation tracks
    for (const auto& track : other.mAnimationTracks)
    {
        mAnimationTracks.push_back(std::make_unique<AnimationTrackClass>(*track));
    }

    mRenderTree.FromTree(other.GetRenderTree(), [&map](const EntityNodeClass* node) {
            return map[node];
        });
}

EntityNodeClass* EntityClass::AddNode(const EntityNodeClass& node)
{
    mNodes.emplace_back(new EntityNodeClass(node));
    return mNodes.back().get();
}
EntityNodeClass* EntityClass::AddNode(EntityNodeClass&& node)
{
    mNodes.emplace_back(new EntityNodeClass(std::move(node)));
    return mNodes.back().get();
}
EntityNodeClass* EntityClass::AddNode(std::unique_ptr<EntityNodeClass> node)
{
    mNodes.push_back(std::move(node));
    return mNodes.back().get();
}

EntityNodeClass& EntityClass::GetNode(size_t index)
{
    ASSERT(index < mNodes.size());
    return *mNodes[index].get();
}
EntityNodeClass* EntityClass::FindNodeByName(const std::string& name)
{
    for (const auto& node : mNodes)
        if (node->GetName() == name)
            return node.get();
    return nullptr;
}
EntityNodeClass* EntityClass::FindNodeById(const std::string& id)
{
    for (const auto& node : mNodes)
        if (node->GetId() == id)
            return node.get();
    return nullptr;
}
const EntityNodeClass& EntityClass::GetNode(size_t index) const
{
    ASSERT(index < mNodes.size());
    return *mNodes[index].get();
}
const EntityNodeClass* EntityClass::FindNodeByName(const std::string& name) const
{
    for (const auto& node : mNodes)
        if (node->GetName() == name)
            return node.get();
    return nullptr;
}
const EntityNodeClass* EntityClass::FindNodeById(const std::string& id) const
{
    for (const auto& node : mNodes)
        if (node->GetId() == id)
            return node.get();
    return nullptr;
}


AnimationTrackClass* EntityClass::AddAnimationTrack(AnimationTrackClass&& track)
{
    mAnimationTracks.push_back(std::make_shared<AnimationTrackClass>(std::move(track)));
    return mAnimationTracks.back().get();
}
AnimationTrackClass* EntityClass::AddAnimationTrack(const AnimationTrackClass& track)
{
    mAnimationTracks.push_back(std::make_shared<AnimationTrackClass>(track));
    return mAnimationTracks.back().get();
}
AnimationTrackClass* EntityClass::AddAnimationTrack(std::unique_ptr<AnimationTrackClass> track)
{
    mAnimationTracks.push_back(std::move(track));
    return mAnimationTracks.back().get();
}
void EntityClass::DeleteAnimationTrack(size_t i)
{
    ASSERT(i < mAnimationTracks.size());
    auto it = mAnimationTracks.begin();
    std::advance(it, i);
    mAnimationTracks.erase(it);
}
bool EntityClass::DeleteAnimationTrackByName(const std::string& name)
{
    for (auto it = mAnimationTracks.begin(); it != mAnimationTracks.end(); ++it)
    {
        if ((*it)->GetName() == name) {
            mAnimationTracks.erase(it);
            return true;
        }
    }
    return false;
}
bool EntityClass::DeleteAnimationTrackById(const std::string& id)
{
    for (auto it = mAnimationTracks.begin(); it != mAnimationTracks.end(); ++it)
    {
        if ((*it)->GetId() == id) {
            mAnimationTracks.erase(it);
            return true;
        }
    }
    return false;
}
AnimationTrackClass& EntityClass::GetAnimationTrack(size_t i)
{
    ASSERT(i < mAnimationTracks.size());
    return *mAnimationTracks[i].get();
}
AnimationTrackClass* EntityClass::FindAnimationTrackByName(const std::string& name)
{
    for (const auto& klass : mAnimationTracks)
    {
        if (klass->GetName() == name)
            return klass.get();
    }
    return nullptr;
}
const AnimationTrackClass& EntityClass::GetAnimationTrack(size_t i) const
{
    ASSERT(i < mAnimationTracks.size());
    return *mAnimationTracks[i].get();
}
const AnimationTrackClass* EntityClass::FindAnimationTrackByName(const std::string& name) const
{
    for (const auto& klass : mAnimationTracks)
    {
        if (klass->GetName() == name)
            return klass.get();
    }
    return nullptr;
}

void EntityClass::LinkChild(EntityNodeClass* parent, EntityNodeClass* child)
{
    game::LinkChild(mRenderTree, parent, child);
}

void EntityClass::BreakChild(EntityNodeClass* child)
{
    game::BreakChild(mRenderTree, child);
}

void EntityClass::ReparentChild(EntityNodeClass* parent, EntityNodeClass* child)
{
    game::ReparentChild(mRenderTree, parent, child);
}

void EntityClass::DeleteNode(EntityNodeClass* node)
{
    game::DeleteNode(mRenderTree, node, mNodes);
}

EntityNodeClass* EntityClass::DuplicateNode(const EntityNodeClass* node)
{
    std::vector<std::unique_ptr<EntityNodeClass>> clones;

    auto* ret = game::DuplicateNode(mRenderTree, node, &clones);
    for (auto& clone : clones)
        mNodes.push_back(std::move(clone));
    return ret;

}

void EntityClass::CoarseHitTest(float x, float y, std::vector<EntityNodeClass*>* hits, std::vector<glm::vec2>* hitbox_positions)
{
    game::CoarseHitTest(mRenderTree, x, y, hits, hitbox_positions);
}
void EntityClass::CoarseHitTest(float x, float y, std::vector<const EntityNodeClass*>* hits, std::vector<glm::vec2>* hitbox_positions) const
{
    game::CoarseHitTest(mRenderTree, x, y, hits, hitbox_positions);
}
glm::vec2 EntityClass::MapCoordsFromNode(float x, float y, const EntityNodeClass* node) const
{
    return game::MapCoordsFromNode(mRenderTree, x, y, node);
}
glm::vec2 EntityClass::MapCoordsToNode(float x, float y, const EntityNodeClass* node) const
{
    return game::MapCoordsToNode(mRenderTree, x, y, node);
}
gfx::FRect EntityClass::GetBoundingRect(const EntityNodeClass* node) const
{
    return game::GetBoundingRect(mRenderTree, node);
}
gfx::FRect EntityClass::GetBoundingRect() const
{
    return game::GetBoundingRect(mRenderTree);
}

FBox EntityClass::GetBoundingBox(const EntityNodeClass* node) const
{
    return game::GetBoundingBox(mRenderTree, node);
}

std::size_t EntityClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
    // include the node hashes in the animation hash
    // this covers both the node values and their traversal order
    mRenderTree.PreOrderTraverseForEach([&](const EntityNodeClass* node) {
        if (node == nullptr)
            return;
        hash = base::hash_combine(hash, node->GetHash());
    });

    for (const auto& track : mAnimationTracks)
        hash = base::hash_combine(hash, track->GetHash());
    return hash;
}

nlohmann::json EntityClass::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "id", mClassId);
    for (const auto& node : mNodes)
    {
        json["nodes"].push_back(node->ToJson());
    }

    for (const auto& track : mAnimationTracks)
    {
        json["tracks"].push_back(track->ToJson());
    }
    json["render_tree"] = mRenderTree.ToJson(game::TreeNodeToJson<EntityNodeClass>);
    return json;
}


// static
std::optional<EntityClass> EntityClass::FromJson(const nlohmann::json& json)
{
    EntityClass ret;
    if (!base::JsonReadSafe(json, "id", &ret.mClassId))
        return std::nullopt;
    if (json.contains("nodes"))
    {
        for (const auto& json : json["nodes"].items())
        {
            std::optional<EntityNodeClass> node = EntityNodeClass::FromJson(json.value());
            if (!node.has_value())
                return std::nullopt;
            ret.mNodes.push_back(std::make_shared<EntityNodeClass>(std::move(node.value())));
        }
    }
    if (json.contains("tracks"))
    {
        for (const auto& js : json["tracks"].items())
        {
            std::optional<AnimationTrackClass> track = AnimationTrackClass::FromJson(js.value());
            if (!track.has_value())
                return std::nullopt;
            ret.mAnimationTracks.push_back(std::make_shared<AnimationTrackClass>(std::move(track.value())));
        }
    }
    ret.mRenderTree.FromJson(json["render_tree"], game::TreeNodeFromJson(ret.mNodes));
    return ret;
}

EntityClass EntityClass::Clone() const
{
    EntityClass ret;

    std::unordered_map<const EntityNodeClass*, const EntityNodeClass*> map;

    // make a deep copy of the nodes.
    for (const auto& node : mNodes)
    {
        auto clone = std::make_unique<EntityNodeClass>(node->Clone());
        map[node.get()] = clone.get();
        ret.mNodes.push_back(std::move(clone));
    }

    // make a deep copy of the animation tracks
    for (const auto& track : mAnimationTracks)
    {
        ret.mAnimationTracks.push_back(std::make_unique<AnimationTrackClass>(track->Clone()));
    }

    ret.mRenderTree.FromTree(mRenderTree, [&map](const EntityNodeClass* node) {
        return map[node];
    });
    return ret;
}

EntityClass& EntityClass::operator=(const EntityClass& other)
{
    if (this == &other)
        return *this;

    EntityClass tmp(other);
    mClassId    = std::move(tmp.mClassId);
    mNodes      = std::move(tmp.mNodes);
    mAnimationTracks = std::move(tmp.mAnimationTracks);
    mRenderTree = tmp.mRenderTree;
    return *this;
}

Entity::Entity(std::shared_ptr<const EntityClass> klass)
    : mClass(klass)
{
    std::unordered_map<const EntityNodeClass*, const EntityNode*> map;

    // build render tree, first create instances of all node classes
    // then build the render tree based on the node instances
    for (size_t i=0; i<mClass->GetNumNodes(); ++i)
    {
        auto node_klass = mClass->GetSharedEntityNodeClass(i);
        auto node_inst  = CreateEntityNodeInstance(node_klass);
        map[node_klass.get()] = node_inst.get();
        mNodes.push_back(std::move(node_inst));
    }

    mRenderTree.FromTree(mClass->GetRenderTree(), [&map](const EntityNodeClass* node) {
            return map[node];
        });

    mInstanceId = base::RandomString(10);
    mFlags.set(Flags::VisibleInGame, true);
}

Entity::Entity(const EntityArgs& args)
  : Entity(args.klass)
{
    mInstanceName = args.name;
    mInstanceId   = args.id;
    mScale        = args.scale;
    mPosition     = args.position;
    mRotation     = args.rotation;
    mLayer        = args.layer;
}

Entity::Entity(const EntityClass& klass)
  : Entity(std::make_shared<EntityClass>(klass))
{}

EntityNode* Entity::AddNode(const EntityNode& node)
{
    mNodes.emplace_back(new EntityNode(node));
    return mNodes.back().get();
}
EntityNode* Entity::AddNode(EntityNode&& node)
{
    mNodes.emplace_back(new EntityNode(std::move(node)));
    return mNodes.back().get();
}

EntityNode* Entity::AddNode(std::unique_ptr<EntityNode> node)
{
    mNodes.push_back(std::move(node));
    return mNodes.back().get();
}

void Entity::LinkChild(EntityNode* parent, EntityNode* child)
{
    game::LinkChild(mRenderTree, parent, child);
}

EntityNode& Entity::GetNode(size_t index)
{
    ASSERT(index < mNodes.size());
    return *mNodes[index].get();
}
EntityNode* Entity::FindNodeByClassName(const std::string& name)
{
    for (auto& node : mNodes)
        if (node->GetClassName() == name)
            return node.get();
    return nullptr;
}
EntityNode* Entity::FindNodeByClassId(const std::string& id)
{
    for (auto& node : mNodes)
        if (node->GetClassId() == id)
            return node.get();
    return nullptr;
}
EntityNode* Entity::FindNodeByInstanceId(const std::string& id)
{
    for (auto& node : mNodes)
        if (node->GetId() == id)
            return node.get();
    return nullptr;
}
EntityNode* Entity::FindNodeByInstanceName(const std::string& name)
{
    for (auto& node : mNodes)
        if (node->GetName() == name)
            return node.get();
    return nullptr;
}

const EntityNode& Entity::GetNode(size_t index) const
{
    ASSERT(index < mNodes.size());
    return *mNodes[index].get();
}
const EntityNode* Entity::FindNodeByClassName(const std::string& name) const
{
    for (auto& node : mNodes)
        if (node->GetClassName() == name)
            return node.get();
    return nullptr;
}
const EntityNode* Entity::FindNodeByClassId(const std::string& id) const
{
    for (auto& node : mNodes)
        if (node->GetClassId() == id)
            return node.get();
    return nullptr;
}
const EntityNode* Entity::FindNodeByInstanceId(const std::string& id) const
{
    for (auto& node : mNodes)
        if (node->GetId() == id)
            return node.get();
    return nullptr;
}
const EntityNode* Entity::FindNodeByInstanceName(const std::string& name) const
{
    for (const auto& node : mNodes)
        if (node->GetName() == name)
            return node.get();
    return nullptr;
}

void Entity::DeleteNode(EntityNode* node)
{
    game::DeleteNode(mRenderTree, node, mNodes);
}

void Entity::CoarseHitTest(float x, float y, std::vector<EntityNode*>* hits, std::vector<glm::vec2>* hitbox_positions)
{
    game::CoarseHitTest(mRenderTree, x, y, hits, hitbox_positions);
}

void Entity::CoarseHitTest(float x, float y, std::vector<const EntityNode*>* hits, std::vector<glm::vec2>* hitbox_positions) const
{
    game::CoarseHitTest(mRenderTree, x, y, hits, hitbox_positions);
}

glm::vec2 Entity::MapCoordsFromNode(float x, float y, const EntityNode* node) const
{
    return game::MapCoordsFromNode(mRenderTree, x, y, node);
}

glm::vec2 Entity::MapCoordsToNode(float x, float y, const EntityNode* node) const
{
    return game::MapCoordsToNode(mRenderTree, x, y, node);
}

glm::mat4 Entity::GetNodeTransform() const
{
    gfx::Transform transform;
    transform.Scale(mScale);
    transform.Rotate(mRotation);
    transform.Translate(mPosition);
    return transform.GetAsMatrix();
}

gfx::FRect Entity::GetBoundingRect(const EntityNode* node) const
{
    return game::GetBoundingRect(mRenderTree, node);
}

gfx::FRect Entity::GetBoundingRect() const
{
    return game::GetBoundingRect(mRenderTree);
}

FBox Entity::GetBoundingBox(const EntityNode* node) const
{
    return game::GetBoundingBox(mRenderTree, node);
}

void Entity::Update(float dt)
{
    mCurrentTime += dt;

    if (!mAnimationTrack)
        return;

    mAnimationTrack->Update(dt);
    for (auto& node : mNodes)
    {
        mAnimationTrack->Apply(*node);
    }

    if (!mAnimationTrack->IsComplete())
        return;

    DEBUG("AnimationTrack '%1' completed.", mAnimationTrack->GetName());

    if (mAnimationTrack->IsLooping())
    {
        mAnimationTrack->Restart();
        for (auto& node : mNodes)
        {
            node->Reset();
        }
        return;
    }
    mAnimationTrack.reset();
}

void Entity::Play(std::unique_ptr<AnimationTrack> track)
{
    // todo: what to do if there's a previous track ?
    // possibilities: reset or queue?
    mAnimationTrack = std::move(track);
}
void Entity::PlayAnimationByName(const std::string& name)
{
    for (size_t i=0; i<mClass->GetNumTracks(); ++i)
    {
        const auto& klass = mClass->GetSharedAnimationTrackClass(i);
        if (klass->GetName() != name)
            continue;
        auto track = std::make_unique<AnimationTrack>(klass);
        Play(std::move(track));
        return;
    }
}
void Entity::PlayAnimationById(const std::string& id)
{
    for (size_t i=0; i<mClass->GetNumTracks(); ++i)
    {
        const auto& klass = mClass->GetSharedAnimationTrackClass(i);
        if (klass->GetId() != id)
            continue;
        auto track = std::make_unique<AnimationTrack>(klass);
        Play(std::move(track));
        return;
    }
}
bool Entity::IsPlaying() const
{
    return !!mAnimationTrack;
}

void Entity::SetScale(const glm::vec2& scale)
{
    for (const auto& node : mNodes)
    {
        if (node->HasRigidBody())
        {
            WARN("Scaling an entity with rigid bodies won't work correctly.");
            break;
        }
    }
    mScale = scale;
}

std::unique_ptr<Entity> CreateEntityInstance(std::shared_ptr<const EntityClass> klass)
{ return std::make_unique<Entity>(klass); }

std::unique_ptr<Entity> CreateEntityInstance(const EntityClass& klass)
{ return CreateEntityInstance(std::make_shared<const EntityClass>(klass)); }

std::unique_ptr<Entity> CreateEntityInstance(const EntityArgs& args)
{ return std::make_unique<Entity>(args); }

std::unique_ptr<EntityNode> CreateEntityNodeInstance(std::shared_ptr<const EntityNodeClass> klass)
{ return std::make_unique<EntityNode>(klass); }

} // namespace
