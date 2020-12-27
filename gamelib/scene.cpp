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

#include <unordered_set>

#include "base/format.h"
#include "gamelib/scene.h"
#include "gamelib/entity.h"
#include "gamelib/treeop.h"
#include "graphics/transform.h"

namespace game
{

std::size_t SceneNodeClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
    hash = base::hash_combine(hash, mEntityId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mPosition);
    hash = base::hash_combine(hash, mScale);
    hash = base::hash_combine(hash, mRotation);
    hash = base::hash_combine(hash, mFlags.value());
    return hash;
}

glm::mat4 SceneNodeClass::GetNodeTransform() const
{
    gfx::Transform transform;
    transform.Scale(mScale);
    transform.Rotate(mRotation);
    transform.Translate(mPosition);
    return transform.GetAsMatrix();
}

SceneNodeClass SceneNodeClass::Clone() const
{
    SceneNodeClass copy(*this);
    copy.mClassId = base::RandomString(10);
    return copy;
}

nlohmann::json SceneNodeClass::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "id",       mClassId);
    base::JsonWrite(json, "entity",   mEntityId);
    base::JsonWrite(json, "name",     mName);
    base::JsonWrite(json, "position", mPosition);
    base::JsonWrite(json, "scale",    mScale);
    base::JsonWrite(json, "rotation", mRotation);
    base::JsonWrite(json, "flags", mFlags.value());
    return json;
}

// static
std::optional<SceneNodeClass> SceneNodeClass::FromJson(const nlohmann::json& json)
{
    unsigned flags = 0;
    SceneNodeClass ret;
    if (!base::JsonReadSafe(json, "id",       &ret.mClassId)||
        !base::JsonReadSafe(json, "entity",   &ret.mEntityId) ||
        !base::JsonReadSafe(json, "name",     &ret.mName) ||
        !base::JsonReadSafe(json, "position", &ret.mPosition) ||
        !base::JsonReadSafe(json, "scale",    &ret.mScale) ||
        !base::JsonReadSafe(json, "rotation", &ret.mRotation) ||
        !base::JsonReadSafe(json, "flags",    &flags))
        return std::nullopt;
    ret.mFlags.set_from_value(flags);
    return ret;
}

SceneClass::SceneClass(const SceneClass& other)
{
    mClassId = other.mClassId;
    for (const auto& node : other.mNodes)
    {
        auto copy = std::make_unique<SceneNodeClass>(*node);
        mNodes.push_back(std::move(copy));
    }
    // use JSON serialization to create a copy of the render tree.
    using Serializer = RenderTreeFunctions<SceneNodeClass>;
    nlohmann::json json = other.mRenderTree.ToJson<Serializer>();

    mRenderTree = RenderTree::FromJson(json, *this).value();
}
SceneNodeClass* SceneClass::AddNode(const SceneNodeClass& node)
{
    mNodes.emplace_back(new SceneNodeClass(node));
    return mNodes.back().get();
}
SceneNodeClass* SceneClass::AddNode(SceneNodeClass&& node)
{
    mNodes.emplace_back(new SceneNodeClass(std::move(node)));
    return mNodes.back().get();
}
SceneNodeClass* SceneClass::AddNode(std::unique_ptr<SceneNodeClass> node)
{
    mNodes.push_back(std::move(node));
    return mNodes.back().get();
}
SceneNodeClass& SceneClass::GetNode(size_t index)
{
    ASSERT(index < mNodes.size());
    return *mNodes[index].get();
}
SceneNodeClass* SceneClass::FindNodeByName(const std::string& name)
{
    for (const auto& node : mNodes)
        if (node->GetName() == name)
            return node.get();
    return nullptr;
}
SceneNodeClass* SceneClass::FindNodeById(const std::string& id)
{
    for (const auto& node : mNodes)
        if (node->GetClassId() == id)
            return node.get();
    return nullptr;
}
const SceneNodeClass& SceneClass::GetNode(size_t index) const
{
    ASSERT(index < mNodes.size());
    return *mNodes[index].get();
}
const SceneNodeClass* SceneClass::FindNodeByName(const std::string& name) const
{
    for (const auto& node : mNodes)
        if (node->GetName() == name)
            return node.get();
    return nullptr;
}
const SceneNodeClass* SceneClass::FindNodeById(const std::string& id) const
{
    for (const auto& node : mNodes)
        if (node->GetClassId() == id)
            return node.get();
    return nullptr;
}

void SceneClass::LinkChild(SceneNodeClass* parent, SceneNodeClass* child)
{
    RenderTreeFunctions<SceneNodeClass>::LinkChild(mRenderTree, parent, child);
}

void SceneClass::BreakChild(SceneNodeClass* child)
{
    RenderTreeFunctions<SceneNodeClass>::BreakChild(mRenderTree, child);
}

void SceneClass::ReparentChild(SceneNodeClass* parent, SceneNodeClass* child)
{
    RenderTreeFunctions<SceneNodeClass>::ReparentChild(mRenderTree, parent, child);
}

void SceneClass::DeleteNode(SceneNodeClass* node)
{
    std::unordered_set<std::string> graveyard;

    RenderTreeFunctions<SceneNodeClass>::DeleteNode(mRenderTree, node, &graveyard);

    // remove each node from the node container
    mNodes.erase(std::remove_if(mNodes.begin(), mNodes.end(), [&graveyard](const auto& node) {
        return graveyard.find(node->GetClassId()) != graveyard.end();
    }), mNodes.end());
}

SceneNodeClass* SceneClass::DuplicateNode(const SceneNodeClass* node)
{
    return RenderTreeFunctions<SceneNodeClass>::DuplicateNode(mRenderTree, node, &mNodes);
}

void SceneClass::CoarseHitTest(float x, float y, std::vector<SceneNodeClass*>* hits,
                   std::vector<glm::vec2>* hitbox_positions)
{
    // todo: improve the implementation with some form of space partitioning.
    class Visitor : public RenderTree::Visitor
    {
    public:
        Visitor(float x, float y,
                std::vector<SceneNodeClass*>& hit_nodes,
                std::vector<glm::vec2>* hit_pos)
          : mHitPos(x, y, 1.0f, 1.0f)
          , mHitNodes(hit_nodes)
          , mHitPositions(hit_pos)
        {}
        virtual void EnterNode(SceneNodeClass* node) override
        {
            if (!node)
                return;
            mTransform.Push(node->GetNodeTransform());
            auto klass = node->GetEntityClass();
            if (!klass) {
                WARN("Node '%1' has no entity class object!", node->GetName());
                return;
            }

            auto scene_to_entity = glm::inverse(mTransform.GetAsMatrix());
            auto entity_hit_pos  = scene_to_entity * mHitPos;
            std::vector<const EntityNodeClass*> nodes;
            klass->CoarseHitTest(entity_hit_pos.x, entity_hit_pos.y, &nodes);
            if (!nodes.empty())
            {
                mHitNodes.push_back(node);
                if (mHitPositions)
                    mHitPositions->push_back(glm::vec2(entity_hit_pos.x, entity_hit_pos.y));
            }
        }
        virtual void LeaveNode(SceneNodeClass* node) override
        {
            if (!node)
                return;
            mTransform.Pop();
        }
    private:
        const glm::vec4 mHitPos;
        std::vector<SceneNodeClass*>& mHitNodes;
        std::vector<glm::vec2>* mHitPositions = nullptr;
        gfx::Transform mTransform;
    };
    Visitor visitor(x, y, *hits, hitbox_positions);
    mRenderTree.PreOrderTraverse(visitor);
}
void SceneClass::CoarseHitTest(float x, float y, std::vector<const SceneNodeClass*>* hits,
                       std::vector<glm::vec2>* hitbox_positions) const
{
    // todo: improve the implementation with some form of space partitioning.
    class Visitor : public RenderTree::ConstVisitor
    {
    public:
        Visitor(float x, float y,
                std::vector<const SceneNodeClass*>& hit_nodes,
                std::vector<glm::vec2>* hit_pos)
                : mHitPos(x, y, 1.0f, 1.0f)
                , mHitNodes(hit_nodes)
                , mHitPositions(hit_pos)
        {}
        virtual void EnterNode(const SceneNodeClass* node) override
        {
            if (!node)
                return;
            mTransform.Push(node->GetNodeTransform());
            auto klass = node->GetEntityClass();
            if (!klass) {
                WARN("Node '%1' has no entity class object!", node->GetName());
                return;
            }

            auto scene_to_entity = glm::inverse(mTransform.GetAsMatrix());
            auto entity_hit_pos  = scene_to_entity * mHitPos;
            std::vector<const EntityNodeClass*> nodes;
            klass->CoarseHitTest(entity_hit_pos.x, entity_hit_pos.y, &nodes);
            if (!nodes.empty())
            {
                mHitNodes.push_back(node);
                if (mHitPositions)
                    mHitPositions->push_back(glm::vec2(entity_hit_pos.x, entity_hit_pos.y));
            }
        }
        virtual void LeaveNode(const SceneNodeClass* node) override
        {
            if (!node)
                return;
            mTransform.Pop();
        }
    private:
        const glm::vec4 mHitPos;
        std::vector<const SceneNodeClass*>& mHitNodes;
        std::vector<glm::vec2>* mHitPositions = nullptr;
        gfx::Transform mTransform;
    };
    Visitor visitor(x, y, *hits, hitbox_positions);
    mRenderTree.PreOrderTraverse(visitor);
}


glm::vec2 SceneClass::MapCoordsFromNode(float x, float y, const SceneNodeClass* node) const
{
    class Visitor : public RenderTree::ConstVisitor
    {
    public:
        Visitor(float x, float y, const SceneNodeClass* node)
          : mPos(x, y, 1.0f, 1.0f)
          , mNode(node)
        {}
        virtual void EnterNode(const SceneNodeClass* node) override
        {
            if (!node)
                return;
            mTransform.Push(node->GetNodeTransform());
            if (node == mNode)
            {
                auto mat = mTransform.GetAsMatrix();
                auto ret = mat * mPos;
                mResult.x = ret.x;
                mResult.y = ret.y;
            }
        }
        virtual void LeaveNode(const SceneNodeClass* node) override
        {
            if (!node)
                return;
            mTransform.Pop();
        }
        glm::vec2 GetResult() const
        { return mResult; }
    private:
        const glm::vec4 mPos;
        const SceneNodeClass* mNode = nullptr;
        gfx::Transform mTransform;
        glm::vec2 mResult;
    };

    Visitor visitor(x, y, node);
    mRenderTree.PreOrderTraverse(visitor);
    return visitor.GetResult();
}
glm::vec2 SceneClass::MapCoordsToNode(float x, float y, const SceneNodeClass* node) const
{
    class Visitor : public RenderTree::ConstVisitor
    {
    public:
        Visitor(float x, float y, const SceneNodeClass* node)
                : mPos(x, y, 1.0f, 1.0f)
                , mNode(node)
        {}
        virtual void EnterNode(const SceneNodeClass* node) override
        {
            if (!node)
                return;
            mTransform.Push(node->GetNodeTransform());
            if (node == mNode)
            {
                auto mat = glm::inverse(mTransform.GetAsMatrix());
                auto ret = mat * mPos;
                mResult.x = ret.x;
                mResult.y = ret.y;
            }
        }
        virtual void LeaveNode(const SceneNodeClass* node) override
        {
            if (!node)
                return;
            mTransform.Pop();
        }
        glm::vec2 GetResult() const
        { return mResult; }
    private:
        const glm::vec4 mPos;
        const SceneNodeClass* mNode = nullptr;
        gfx::Transform mTransform;
        glm::vec2 mResult;
    };

    Visitor visitor(x, y, node);
    mRenderTree.PreOrderTraverse(visitor);
    return visitor.GetResult();
}

size_t SceneClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
    // include the node hashes in the animation hash
    // this covers both the node values and their traversal order
    mRenderTree.PreOrderTraverseForEach([&](const SceneNodeClass* node) {
        if (node == nullptr)
            return;
        hash = base::hash_combine(hash, node->GetHash());
    });
    return hash;
}

SceneNodeClass* SceneClass::TreeNodeFromJson(const nlohmann::json& json)
{
    if (!json.contains("id")) // root node has no id
        return nullptr;

    const std::string& id = json["id"];
    for (auto& it : mNodes)
        if (it->GetClassId() == id) return it.get();

    BUG("No such node found.");
}
nlohmann::json SceneClass::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "id", mClassId);
    for (const auto& node : mNodes)
    {
        json["nodes"].push_back(node->ToJson());
    }
    using Serializer = RenderTreeFunctions<SceneNodeClass>;
    json["render_tree"] = mRenderTree.ToJson<Serializer>();
    return json;
}

// static
std::optional<SceneClass> SceneClass::FromJson(const nlohmann::json& json)
{
    SceneClass ret;
    if (!base::JsonReadSafe(json, "id", &ret.mClassId))
        return std::nullopt;
    if (json.contains("nodes"))
    {
        for (const auto& json : json["nodes"].items())
        {
            std::optional<SceneNodeClass> node = SceneNodeClass::FromJson(json.value());
            if (!node.has_value())
                return std::nullopt;
            ret.mNodes.push_back(std::make_unique<SceneNodeClass>(std::move(node.value())));
        }
    }
    auto& serializer = ret;

    auto render_tree = RenderTree::FromJson(json["render_tree"], serializer);
    if (!render_tree.has_value())
        return std::nullopt;
    ret.mRenderTree = std::move(render_tree.value());
    return ret;
}
SceneClass SceneClass::Clone() const
{
    SceneClass ret;

    struct Serializer {
        SceneNodeClass* TreeNodeFromJson(const nlohmann::json& json)
        {
            if (!json.contains("id")) // root node has no id
                return nullptr;
            const std::string& old_id = json["id"];
            const std::string& new_id = idmap[old_id];
            auto* ret = nodes[new_id];
            ASSERT(ret != nullptr && "No such node found.");
            return ret;
        }
        std::unordered_map<std::string, std::string> idmap;
        std::unordered_map<std::string, SceneNodeClass*> nodes;
    };
    Serializer serializer;

    // make a deep copy of the nodes.
    for (const auto& node : mNodes)
    {
        auto clone = std::make_unique<SceneNodeClass>(*node);
        serializer.idmap[node->GetClassId()]  = clone->GetClassId();
        serializer.nodes[clone->GetClassId()] = clone.get();
        ret.mNodes.push_back(std::move(clone));
    }

    // use the json serialization setup the copy of the
    // render tree.
    using Serializer2 = RenderTreeFunctions<SceneNodeClass>;
    nlohmann::json json = mRenderTree.ToJson<Serializer2>();
    // build our render tree.
    ret.mRenderTree = RenderTree::FromJson(json, serializer).value();
    return ret;
}

SceneClass& SceneClass::operator=(const SceneClass& other)
{
    if (this == &other)
        return *this;

    SceneClass tmp(other);
    mClassId    = std::move(tmp.mClassId);
    mNodes      = std::move(tmp.mNodes);
    mRenderTree = tmp.mRenderTree;
    return *this;
}

Scene::Scene(std::shared_ptr<const SceneClass> klass)
  : mClass(klass)
{
    for (size_t i=0; i<klass->GetNumNodes(); ++i)
    {
        const auto& node = klass->GetNode(i);
        EntityArgs args;
        args.klass    = node.GetEntityClass();
        args.rotation = node.GetRotation();
        args.position = node.GetTranslation();
        args.scale    = node.GetScale();
        args.name     = node.GetName();
        args.id       = node.GetClassId();
        ASSERT(args.klass);
        auto entity   = CreateEntityInstance(args);
        mEntities.push_back(std::move(entity));
    }
    // rebuild the render tree through JSON serialization
    using Serializer2 = RenderTreeFunctions<SceneNodeClass>;
    nlohmann::json json = mClass->GetRenderTree().ToJson<Serializer2>();

    struct Serializer {
        Entity* TreeNodeFromJson(const nlohmann::json& json)
        {
            if (!json.contains("id")) // root node has no id
                return nullptr;
            const std::string& id = json["id"];
            for (auto& it : mScene->mEntities)
            {
                if (it->GetInstanceId() == id)
                    return it.get();
            }
            return nullptr;
        }
        Scene* mScene = nullptr;
    };
    Serializer serializer;
    serializer.mScene = this;
    mRenderTree = RenderTree::FromJson(json, serializer).value();
}

Scene::Scene(const SceneClass& klass) : Scene(std::make_shared<SceneClass>(klass))
{}

Entity& Scene::GetEntity(size_t index)
{
    ASSERT(index < mEntities.size());
    return *mEntities[index];
}
Entity* Scene::FindEntityByInstanceId(const std::string& id)
{
    for (auto& e : mEntities)
        if (e->GetInstanceId() == id)
            return e.get();
    return nullptr;
}
Entity* Scene::FindEntityByInstanceName(const std::string& name)
{
    for (auto& e : mEntities)
        if (e->GetInstanceName() == name)
            return e.get();
    return nullptr;
}

const Entity& Scene::GetEntity(size_t index) const
{
    ASSERT(index < mEntities.size());
    return *mEntities[index];
}
const Entity* Scene::FindEntityByInstanceId(const std::string& id) const
{
    for (const auto& e : mEntities)
        if (e->GetInstanceId() == id)
            return e.get();
    return nullptr;
}
const Entity* Scene::FindEntityByInstanceName(const std::string& name) const
{
    for (const auto& e : mEntities)
        if (e->GetInstanceName() == name)
            return e.get();
    return nullptr;
}

std::unique_ptr<Scene> CreateSceneInstance(std::shared_ptr<const SceneClass> klass)
{
    return std::make_unique<Scene>(klass);
}

std::unique_ptr<Scene> CreateSceneInstance(const SceneClass& klass)
{
    return std::make_unique<Scene>(klass);
}

} // namespace