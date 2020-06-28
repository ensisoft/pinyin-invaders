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

#include <map>

#include "base/logging.h"
#include "base/assert.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "graphics/drawing.h"
#include "graphics/painter.h"
#include "graphics/transform.h"
#include "animation.h"
#include "gfxfactory.h"

namespace game
{

AnimationNode::AnimationNode()
{
    mId = base::RandomString(10);
    mBitFlags.set(Flags::VisibleInEditor, true);
}

bool AnimationNode::Update(float dt)
{
    // disable this for now, need to figure out the time/timeline
    // related functionality.
    /*
    mTime += dt;
    if (mTime < mStartTime)
        return true;
    if (mTime - mStartTime > mLifetime)
        return false;
    */
    mTime += dt;

    if (mDrawable)
        mDrawable->Update(dt);

    if (mMaterial)
        mMaterial->SetRuntime(mTime - mStartTime);

    return true;
}

void AnimationNode::Reset()
{
    mTime = 0.0f;
}

glm::mat4 AnimationNode::GetNodeTransform() const
{
    // transformation order is the order they're
    // written here.
    gfx::Transform transform;
    // transform.Scale(mScale); // currently not supported.
    transform.Rotate(mRotation);
    transform.Translate(mPosition);
    return transform.GetAsMatrix();
}

glm::mat4 AnimationNode::GetModelTransform() const
{
    gfx::Transform transform;
    transform.Scale(mSize);
    // offset the object so that the center of the shape is aligned
    // with the position parameter.
    transform.Translate(-mSize.x * 0.5f, -mSize.y * 0.5f);
    return transform.GetAsMatrix();
}

bool AnimationNode::Prepare(const GfxFactory& loader)
{
    //            == About resource loading ==
    // User defined resources have a combination of type and name
    // where type is the underlying class type and name identifies
    // the set of resources that the user edits that instances of that
    // said type then use.
    // Primitive / (non user defined resources) don't need a name
    // since the name is irrelevant since the objects are stateless
    // in this sense and don't have properties that would change between instances.
    // For example with drawable rectangles (gfx::Rectangle) all rectangles
    // that we might want to draw are basically the same. We don't need to
    // configure each rectangle object with properties that would distinguish
    // it from other rectangles.
    // In fact there's basically even no need to create more than 1 instance
    // of such resource and then share it between all users.
    //
    // User defined resources on the other hand *can be* unique.
    // For example particle engines, the underlying object type is same for
    // particle engine A and B, but their defining properties are completely
    // different. To distinguish the set of properties the user gives each
    // particle engine a "name". Then when loading such objects we must
    // load them by name. Additionally the resources may or may not be
    // shared. For example when a fleet of alien spaceships are rendered
    // each spaceship might have their own particle engine (own simulation state)
    // thus producing a unique rendering for each spaceship. However the problem
    // is that this might be computationally heavy. For N ships we'd need to do
    // N particle engine simulations.
    // However it's also possible that the particle engines are shared and each
    // ship (of the same type) just refers to the same particle engine. Then
    // each ship will render the same particle stream.
    mDrawable = loader.MakeDrawable(mDrawableName);
    mMaterial = loader.MakeMaterial(mMaterialName);

    if (mDrawable)
    {
        mDrawable->SetStyle(mRenderStyle);
        mDrawable->SetLineWidth(mLineWidth);
    }
    return mDrawable && mMaterial;
}

nlohmann::json AnimationNode::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "id", mId);
    base::JsonWrite(json, "name", mName);
    base::JsonWrite(json, "material", mMaterialName);
    base::JsonWrite(json, "drawable", mDrawableName);
    base::JsonWrite(json, "position", mPosition);
    base::JsonWrite(json, "size", mSize);
    base::JsonWrite(json, "scale", mScale);
    base::JsonWrite(json, "rotation", mRotation);
    base::JsonWrite(json, "layer", mLayer);
    base::JsonWrite(json, "render_pass", mRenderPass);
    base::JsonWrite(json, "render_style", mRenderStyle);
    base::JsonWrite(json, "linewidth", mLineWidth);
    base::JsonWrite(json, "bitflags", mBitFlags.value());
    return json;
}

// static
std::optional<AnimationNode> AnimationNode::FromJson(const nlohmann::json& object)
{
    AnimationNode ret;
    if (!base::JsonReadSafe(object, "id", &ret.mId) ||
        !base::JsonReadSafe(object, "name", &ret.mName) ||
        !base::JsonReadSafe(object, "material", &ret.mMaterialName) ||
        !base::JsonReadSafe(object, "drawable", &ret.mDrawableName) ||
        !base::JsonReadSafe(object, "position", &ret.mPosition) ||
        !base::JsonReadSafe(object, "size", &ret.mSize) ||
        !base::JsonReadSafe(object, "scale", &ret.mScale) ||
        !base::JsonReadSafe(object, "rotation", &ret.mRotation) ||
        !base::JsonReadSafe(object, "layer", &ret.mLayer) ||
        !base::JsonReadSafe(object, "render_pass", &ret.mRenderPass))
        return std::nullopt;
    base::JsonReadSafe(object, "render_style", &ret.mRenderStyle);
    base::JsonReadSafe(object, "linewidth", &ret.mLineWidth);

    std::uint32_t bitflags = 0;
    if (!base::JsonReadSafe(object, "bitflags", &bitflags))
    {
        // default flag values be here.
        ret.mBitFlags.set(Flags::VisibleInEditor, true);
    }
    ret.mBitFlags.set_from_value(bitflags);

    return ret;
}

Animation::Animation(const Animation& other)
{
    // make a deep copy of the nodes.
    for (const auto& node : other.mNodes)
    {
        mNodes.push_back(std::make_unique<AnimationNode>(*node));
    }

    // use the json serialization setup the copy of the
    // render tree.
    nlohmann::json json = other.mRenderTree.ToJson(other);
    // build our render tree.
    mRenderTree = RenderTree::FromJson(json, *this).value();
}

void Animation::Draw(gfx::Painter& painter, gfx::Transform& transform, DrawHook* hook) const
{
    // here we could apply operations that would apply to the whole
    // animation but currently we don't need such things.
    // if we did we could begin new transformation scope for this
    // by pushing a new scope in the transformation stack.
    // transfrom.Push();
    std::vector<DrawPacket> packets;

    class Visitor : public RenderTree::ConstVisitor
    {
    public:
        Visitor(std::vector<DrawPacket>& packets, gfx::Transform& transform, DrawHook* hook)
            : mPackets(packets)
            , mTransform(transform)
            , mHook(hook)
        {}
        virtual void EnterNode(const AnimationNode* node) override
        {
            if (!node)
                return;

            mTransform.Push(node->GetNodeTransform());
            mTransform.Push(node->GetModelTransform());

            DrawPacket packet;
            packet.material  = node->GetMaterial();
            packet.drawable  = node->GetDrawable();
            packet.layer     = node->GetLayer();
            packet.transform = mTransform.GetAsMatrix();

            if (!mHook || (mHook && mHook->InspectPacket(node, packet)))
                mPackets.push_back(packet);

            // pop the model transform
            mTransform.Pop();

            // append any extra packets if needed.
            if (mHook)
            {
                const auto num_transforms = mTransform.GetNumTransforms();

                std::vector<DrawPacket> packets;
                mHook->AppendPackets(node, mTransform, packets);
                for (auto& p : packets)
                    mPackets.push_back(std::move(p));

                // make sure the stack is popped properly.
                ASSERT(mTransform.GetNumTransforms() == num_transforms);
            }
        }
        virtual void LeaveNode(const AnimationNode* node) override
        {
            if (!node)
                return;

            mTransform.Pop();
        }
    private:
        std::vector<DrawPacket>& mPackets;
        gfx::Transform& mTransform;
        DrawHook* mHook = nullptr;
    };

    Visitor visitor(packets, transform, hook);
    mRenderTree.PreOrderTraverse(visitor);

    // implement "layers" by drawing in a sorted order as determined
    // by node's layer value.
    std::sort(packets.begin(), packets.end(), [](const auto& a, const auto& b) {
            return a.layer < b.layer;
        });

    for (const auto& packet : packets)
    {
        if (!packet.material || !packet.drawable)
            continue;

        painter.Draw(*packet.drawable, gfx::Transform(packet.transform), *packet.material);
    }

    // if we used a new trańsformation scope pop it here.
    //transform.Pop();
}

bool Animation::Update(float dt)
{
    bool alive = false;

    for (auto& node : mNodes)
    {
        if (node->Update(dt))
            alive = true;
    }
    return alive;
}

bool Animation::IsExpired() const
{
    return false;
}

AnimationNode* Animation::AddNode(AnimationNode&& node)
{
#if true
    for (const auto& old : mNodes)
    {
        ASSERT(old->GetId() != node.GetId());
    }
#endif

    mNodes.push_back(std::make_unique<AnimationNode>(std::move(node)));
    return mNodes.back().get();
}
// Add a new animation node. Returns a pointer to the node
// that was added to the anímation.
AnimationNode* Animation::AddNode(const AnimationNode& node)
{
#if true
    for (const auto& old : mNodes)
    {
        ASSERT(old->GetId() != node.GetId());
    }
#endif

    mNodes.push_back(std::make_unique<AnimationNode>(node));
    return mNodes.back().get();
}

void Animation::DeleteNodeByIndex(size_t i)
{
    ASSERT(i < mNodes.size());
    auto it = std::begin(mNodes);
    std::advance(it, i);
    mNodes.erase(it);
}
// Delete a node by the given id.
void Animation::DeleteNodeById(const std::string& id)
{
    for (auto it = mNodes.begin(); it != mNodes.end(); ++it)
    {
        if ((*it)->GetId() == id)
        {
            mNodes.erase(it);
            return;
        }
    }
    ASSERT(!"No such node found.");
}


nlohmann::json Animation::ToJson() const
{
    nlohmann::json json;
    for (const auto& node : mNodes)
    {
        json["nodes"].push_back(node->ToJson());
    }

    using Serializer = Animation;

    json["render_tree"] = mRenderTree.ToJson<Serializer>();
    return json;
}

AnimationNode* Animation::TreeNodeFromJson(const nlohmann::json& json)
{
    if (!json.contains("id")) // root node has no id
        return nullptr;

    const std::string& id = json["id"];
    for (auto& it : mNodes)
        if (it->GetId() == id) return it.get();

    ASSERT(!"no such node found.");
    return nullptr;
}


// static
std::optional<Animation> Animation::FromJson(const nlohmann::json& object)
{
    Animation ret;

    if (object.contains("nodes"))
    {
        for (const auto& json : object["nodes"].items())
        {
            std::optional<AnimationNode> comp = AnimationNode::FromJson(json.value());
            if (!comp.has_value())
                return std::nullopt;
            auto node = std::make_unique<AnimationNode>(std::move(comp.value()));
            ret.mNodes.push_back(std::move(node));
        }
    }

    auto& serializer = ret;

    auto render_tree = RenderTree::FromJson(object["render_tree"], serializer);
    if (!render_tree.has_value())
        return std::nullopt;
    ret.mRenderTree = std::move(render_tree.value());
    return ret;
}

// static
nlohmann::json Animation::TreeNodeToJson(const AnimationNode* node)
{
    // do only shallow serialization of the animation node,
    // i.e. only record the id so that we can restore the node
    // later on load based on the ID.
    nlohmann::json ret;
    if (node)
        ret["id"] = node->GetId();
    return ret;
}


void Animation::Reset()
{
    for (auto& node : mNodes)
    {
        node->Reset();
    }
}

AnimationNode* Animation::CoarseHitTest(float x, float y, glm::vec2* hitbox_pos)
{
    class Visitor : public RenderTree::Visitor
    {
    public:
        Visitor(const glm::vec4& point) : mHitPoint(point)
        {}

        virtual void EnterNode(AnimationNode* node) override
        {
            if (!node)
                return;

            mTransform.Push(node->GetNodeTransform());
            // using the model transform will put the coordinates in the drawable coordinate
            // space, i.e. normalized coordinates.
            mTransform.Push(node->GetModelTransform());

            const auto& animation_to_node = glm::inverse(mTransform.GetAsMatrix());
            const auto& hitpoint_in_node = animation_to_node * mHitPoint;

            if (hitpoint_in_node.x >= 0.0f &&
                hitpoint_in_node.x < 1.0f &&
                hitpoint_in_node.y >= 0.0f &&
                hitpoint_in_node.y < 1.0f)
            {
                const auto& size = node->GetSize();
                Hit hit;
                hit.node = node;
                hit.point = glm::vec2(hitpoint_in_node.x * size.x, hitpoint_in_node.y * size.y);
                mHitNodes.push_back(hit);
            }
            mTransform.Pop();
        }
        virtual void LeaveNode(AnimationNode* node) override
        {
            if (!node)
                return;

            mTransform.Pop();
        }

        AnimationNode* GetBestMatch(glm::vec2* hitbox_pos)
        {
            if (mHitNodes.empty())
                return nullptr;

            auto match = mHitNodes[0];
            for (size_t i=1; i<mHitNodes.size(); ++i)
            {
                if (mHitNodes[i].node->GetLayer() > match.node->GetLayer())
                    match = mHitNodes[i];
            }
            if (hitbox_pos)
                *hitbox_pos = match.point;
            return match.node;
        }

    private:
        const glm::vec4 mHitPoint;
        gfx::Transform mTransform;
        struct Hit {
            AnimationNode* node = nullptr;
            glm::vec2 point;
        };
        std::vector<Hit> mHitNodes;
    };

    Visitor visitor(glm::vec4(x, y, 1.0f, 1.0f));
    mRenderTree.PreOrderTraverse(visitor);

    return visitor.GetBestMatch(hitbox_pos);
}

glm::vec2 Animation::MapCoordsFromNode(float x, float y, const AnimationNode* node) const
{
    class Visitor : public RenderTree::ConstVisitor
    {
    public:
        Visitor(float x, float y, const AnimationNode* node)
          : mX(x), mY(y), mNode(node)
        {}
        virtual void EnterNode(const AnimationNode* node) override
        {
            if (!node)
                return;

            mTransform.Push(node->GetNodeTransform());

            // if it's the node we're interested in
            if (node == mNode)
            {
                const auto& mat = mTransform.GetAsMatrix();
                const auto& vec = mat * glm::vec4(mX, mY, 1.0f, 1.0f);
                mResult = glm::vec2(vec.x, vec.y);
            }
        }

        virtual void LeaveNode(const AnimationNode* node) override
        {
            if (!node)
                return;
            mTransform.Pop();
        }

        glm::vec2 GetResult() const
        { return mResult; }
    private:
        const float mX = 0.0f;
        const float mY = 0.0f;
        const AnimationNode* mNode = nullptr;
        glm::vec2 mResult;
        gfx::Transform mTransform;
    };

    Visitor visitor(x, y, node);
    mRenderTree.PreOrderTraverse(visitor);

    return visitor.GetResult();
}

glm::vec2 Animation::MapCoordsToNode(float x, float y, const AnimationNode* node) const
{
    class Visitor : public RenderTree::ConstVisitor
    {
    public:
        Visitor(float x, float y, const AnimationNode* node)
          : mCoords(x, y, 1.0f, 1.0f)
          , mNode(node)
        {}
        virtual void EnterNode(const AnimationNode* node) override
        {
            if (!node)
                return;
            mTransform.Push(node->GetNodeTransform());

            if (node == mNode)
            {
                const auto& animation_to_node = glm::inverse(mTransform.GetAsMatrix());
                const auto& vec = animation_to_node * mCoords;
                mResult = glm::vec2(vec.x, vec.y);
            }
        }
        virtual void LeaveNode(const AnimationNode* node) override
        {
            if (!node)
                return;
            mTransform.Pop();
        }
        glm::vec2 GetResult() const
        { return mResult; }

    private:
        const glm::vec4 mCoords;
        const AnimationNode* mNode = nullptr;
        glm::vec2 mResult;
        gfx::Transform mTransform;
    };

    Visitor visitor(x, y, node);
    mRenderTree.PreOrderTraverse(visitor);

    return visitor.GetResult();
}

void Animation::Prepare(const GfxFactory& loader)
{
    for (auto& node : mNodes)
    {
        if (!node->Prepare(loader))
        {
            WARN("Node '%1' failed to prepare.", node->GetName());
        }
    }
}

Animation& Animation::operator=(const Animation& other)
{
    if (this == &other)
        return *this;

    Animation copy(other);

    for (auto& node : copy.mNodes)
        mNodes.push_back(std::move(node));

    mRenderTree = copy.mRenderTree;
    return *this;
}

} // namespace