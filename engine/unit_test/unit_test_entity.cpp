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
#include "warnpop.h"

#include <string>
#include <cstddef>
#include <iostream>

#include "base/test_minimal.h"
#include "base/test_float.h"
#include "base/assert.h"
#include "base/math.h"
#include "graphics/color4f.h"
#include "graphics/material.h"
#include "graphics/drawable.h"
#include "engine/entity.h"


bool operator==(const glm::vec2& lhs, const glm::vec2& rhs)
{
    return real::equals(lhs.x, rhs.x) &&
           real::equals(lhs.y, rhs.y);
}

// build easily comparable representation of the render tree
// by concatenating node names into a string in the order
// of traversal.
std::string WalkTree(const game::EntityClass& entity)
{
    std::string names;
    const auto& tree = entity.GetRenderTree();
    tree.PreOrderTraverseForEach([&names](const auto* node)  {
        if (node) {
            names.append(node->GetName());
            names.append(" ");
        }
    });
    if (!names.empty())
        names.pop_back();
    return names;
}

std::string WalkTree(const game::Entity& entity)
{
    std::string names;
    const auto& tree = entity.GetRenderTree();
    tree.PreOrderTraverseForEach([&names](const auto* node)  {
        if (node) {
            names.append(node->GetName());
            names.append(" ");
        }
    });
    if (!names.empty())
        names.pop_back();
    return names;
}

void unit_test_entity_node()
{
    game::DrawableItemClass draw;
    draw.SetDrawableId("rectangle");
    draw.SetMaterialId("test");
    draw.SetRenderPass(game::DrawableItemClass::RenderPass::Mask);
    draw.SetFlag(game::DrawableItemClass::Flags::UpdateDrawable, true);
    draw.SetFlag(game::DrawableItemClass::Flags::RestartDrawable, false);
    draw.SetLayer(10);
    draw.SetLineWidth(5.0f);

    game::RigidBodyItemClass body;
    body.SetCollisionShape(game::RigidBodyItemClass::CollisionShape::Circle);
    body.SetSimulation(game::RigidBodyItemClass::Simulation::Dynamic);
    body.SetFlag(game::RigidBodyItemClass::Flags::Bullet, true);
    body.SetFriction(2.0f);
    body.SetRestitution(3.0f);
    body.SetAngularDamping(4.0f);
    body.SetLinearDamping(5.0f);
    body.SetDensity(-1.0f);
    body.SetAngularVelocity(5.0f);
    body.SetLinearVelocity(glm::vec2(-1.0f, -2.0f));
    body.SetPolygonShapeId("shape");

    game::EntityNodeClass node;
    node.SetName("root");
    node.SetSize(glm::vec2(100.0f, 100.0f));
    node.SetTranslation(glm::vec2(150.0f, -150.0f));
    node.SetScale(glm::vec2(4.0f, 5.0f));
    node.SetRotation(1.5f);
    node.SetDrawable(draw);
    node.SetRigidBody(body);

    TEST_REQUIRE(node.HasDrawable());
    TEST_REQUIRE(node.HasRigidBody());
    TEST_REQUIRE(node.GetName()         == "root");
    TEST_REQUIRE(node.GetSize()         == glm::vec2(100.0f, 100.0f));
    TEST_REQUIRE(node.GetTranslation()  == glm::vec2(150.0f, -150.0f));
    TEST_REQUIRE(node.GetScale()        == glm::vec2(4.0f, 5.0f));
    TEST_REQUIRE(node.GetRotation()     == real::float32(1.5f));
    TEST_REQUIRE(node.GetDrawable()->GetLineWidth()    == real::float32(5.0f));
    TEST_REQUIRE(node.GetDrawable()->GetRenderPass()   == game::DrawableItemClass::RenderPass::Mask);
    TEST_REQUIRE(node.GetDrawable()->GetLayer()        == 10);
    TEST_REQUIRE(node.GetDrawable()->GetDrawableId()   == "rectangle");
    TEST_REQUIRE(node.GetDrawable()->GetMaterialId()   == "test");
    TEST_REQUIRE(node.GetDrawable()->TestFlag(game::DrawableItemClass::Flags::UpdateDrawable) == true);
    TEST_REQUIRE(node.GetDrawable()->TestFlag(game::DrawableItemClass::Flags::RestartDrawable) == false);
    TEST_REQUIRE(node.GetRigidBody()->GetCollisionShape() == game::RigidBodyItemClass::CollisionShape::Circle);
    TEST_REQUIRE(node.GetRigidBody()->GetSimulation() == game::RigidBodyItemClass::Simulation::Dynamic);
    TEST_REQUIRE(node.GetRigidBody()->TestFlag(game::RigidBodyItemClass::Flags::Bullet));
    TEST_REQUIRE(node.GetRigidBody()->GetFriction()       == real::float32(2.0f));
    TEST_REQUIRE(node.GetRigidBody()->GetRestitution()    == real::float32(3.0f));
    TEST_REQUIRE(node.GetRigidBody()->GetAngularDamping() == real::float32(4.0f));
    TEST_REQUIRE(node.GetRigidBody()->GetLinearDamping()  == real::float32(5.0f));
    TEST_REQUIRE(node.GetRigidBody()->GetDensity()        == real::float32(-1.0));
    TEST_REQUIRE(node.GetRigidBody()->GetAngularVelocity()== real::float32(5.0f));
    TEST_REQUIRE(node.GetRigidBody()->GetLinearVelocity() == glm::vec2(-1.0f, -2.0f));
    TEST_REQUIRE(node.GetRigidBody()->GetPolygonShapeId() == "shape");

    // to/from json
    {
        auto ret = game::EntityNodeClass::FromJson(node.ToJson());
        TEST_REQUIRE(ret.has_value());
        TEST_REQUIRE(ret->HasDrawable());
        TEST_REQUIRE(ret->HasRigidBody());
        TEST_REQUIRE(ret->GetName()         == "root");
        TEST_REQUIRE(ret->GetSize()         == glm::vec2(100.0f, 100.0f));
        TEST_REQUIRE(ret->GetTranslation()  == glm::vec2(150.0f, -150.0f));
        TEST_REQUIRE(ret->GetScale()        == glm::vec2(4.0f, 5.0f));
        TEST_REQUIRE(ret->GetRotation()     == real::float32(1.5f));
        TEST_REQUIRE(ret->GetDrawable()->GetDrawableId()   == "rectangle");
        TEST_REQUIRE(ret->GetDrawable()->GetMaterialId()   == "test");
        TEST_REQUIRE(ret->GetDrawable()->GetLineWidth()    == real::float32(5.0f));
        TEST_REQUIRE(ret->GetDrawable()->GetRenderPass()   == game::DrawableItemClass::RenderPass::Mask);
        TEST_REQUIRE(ret->GetDrawable()->TestFlag(game::DrawableItemClass::Flags::UpdateDrawable) == true);
        TEST_REQUIRE(ret->GetDrawable()->TestFlag(game::DrawableItemClass::Flags::RestartDrawable) == false);
        TEST_REQUIRE(node.GetRigidBody()->GetCollisionShape() == game::RigidBodyItemClass::CollisionShape::Circle);
        TEST_REQUIRE(node.GetRigidBody()->GetSimulation() == game::RigidBodyItemClass::Simulation::Dynamic);
        TEST_REQUIRE(node.GetRigidBody()->TestFlag(game::RigidBodyItemClass::Flags::Bullet));
        TEST_REQUIRE(node.GetRigidBody()->GetFriction()       == real::float32(2.0f));
        TEST_REQUIRE(node.GetRigidBody()->GetRestitution()    == real::float32(3.0f));
        TEST_REQUIRE(node.GetRigidBody()->GetAngularDamping() == real::float32(4.0f));
        TEST_REQUIRE(node.GetRigidBody()->GetLinearDamping()  == real::float32(5.0f));
        TEST_REQUIRE(node.GetRigidBody()->GetDensity()        == real::float32(-1.0));
        TEST_REQUIRE(node.GetRigidBody()->GetPolygonShapeId() == "shape");
        TEST_REQUIRE(ret->GetHash() == node.GetHash());
    }

    // test copy and copy ctor
    {
        auto copy(node);
        TEST_REQUIRE(copy.GetHash() == node.GetHash());
        TEST_REQUIRE(copy.GetId() == node.GetId());
        game::EntityNodeClass temp;
        temp = copy;
        TEST_REQUIRE(temp.GetHash() == node.GetHash());
        TEST_REQUIRE(temp.GetId() == node.GetId());
    }

    // test clone
    {
        auto clone = node.Clone();
        TEST_REQUIRE(clone.GetHash() != node.GetHash());
        TEST_REQUIRE(clone.GetId() != node.GetId());
        TEST_REQUIRE(clone.GetName()         == "root");
        TEST_REQUIRE(clone.GetSize()         == glm::vec2(100.0f, 100.0f));
        TEST_REQUIRE(clone.GetTranslation()  == glm::vec2(150.0f, -150.0f));
        TEST_REQUIRE(clone.GetScale()        == glm::vec2(4.0f, 5.0f));
        TEST_REQUIRE(clone.GetRotation()     == real::float32(1.5f));
        TEST_REQUIRE(clone.GetDrawable()->GetDrawableId()   == "rectangle");
        TEST_REQUIRE(clone.GetDrawable()->GetMaterialId()   == "test");
        TEST_REQUIRE(clone.GetDrawable()->GetLineWidth()    == real::float32(5.0f));
        TEST_REQUIRE(clone.GetDrawable()->GetRenderPass()   == game::DrawableItemClass::RenderPass::Mask);
        TEST_REQUIRE(clone.GetDrawable()->TestFlag(game::DrawableItemClass::Flags::UpdateDrawable) == true);
        TEST_REQUIRE(clone.GetDrawable()->TestFlag(game::DrawableItemClass::Flags::RestartDrawable) == false);
    }

    // test instance state.
    {
        // check initial state.
        game::EntityNode instance(node);
        TEST_REQUIRE(instance.GetId() != node.GetId());
        TEST_REQUIRE(instance.GetName()         == "root");
        TEST_REQUIRE(instance.GetClassName()    == "root");
        TEST_REQUIRE(instance.GetSize()         == glm::vec2(100.0f, 100.0f));
        TEST_REQUIRE(instance.GetTranslation()  == glm::vec2(150.0f, -150.0f));
        TEST_REQUIRE(instance.GetScale()        == glm::vec2(4.0f, 5.0f));
        TEST_REQUIRE(instance.GetRotation()     == real::float32(1.5f));
        TEST_REQUIRE(instance.HasRigidBody());
        TEST_REQUIRE(instance.HasDrawable());
        TEST_REQUIRE(instance.GetDrawable()->GetLineWidth()   == real::float32(5.0f));
        TEST_REQUIRE(instance.GetDrawable()->GetRenderPass()  == game::DrawableItemClass::RenderPass::Mask);
        TEST_REQUIRE(instance.GetRigidBody()->GetPolygonShapeId() == "shape");

        instance.SetName("foobar");
        instance.SetSize(glm::vec2(200.0f, 200.0f));
        instance.SetTranslation(glm::vec2(350.0f, -350.0f));
        instance.SetScale(glm::vec2(1.0f, 1.0f));
        instance.SetRotation(2.5f);
        TEST_REQUIRE(instance.GetName()        == "foobar");
        TEST_REQUIRE(instance.GetSize()        == glm::vec2(200.0f, 200.0f));
        TEST_REQUIRE(instance.GetTranslation() == glm::vec2(350.0f, -350.0f));
        TEST_REQUIRE(instance.GetScale()       == glm::vec2(1.0f, 1.0f));
        TEST_REQUIRE(instance.GetRotation()    == real::float32(2.5f));
    }
}

void unit_test_entity_class()
{
    game::EntityClass entity;
    entity.SetName("TestEntityClass");
    {
        game::EntityNodeClass node;
        node.SetName("root");
        node.SetTranslation(glm::vec2(10.0f, 10.0f));
        node.SetSize(glm::vec2(10.0f, 10.0f));
        node.SetScale(glm::vec2(1.0f, 1.0f));
        node.SetRotation(0.0f);
        entity.AddNode(std::move(node));
    }
    {
        game::EntityNodeClass node;
        node.SetName("child_1");
        node.SetTranslation(glm::vec2(10.0f, 10.0f));
        node.SetSize(glm::vec2(2.0f, 2.0f));
        node.SetScale(glm::vec2(1.0f, 1.0f));
        node.SetRotation(0.0f);
        entity.AddNode(std::move(node));
    }
    {
        game::EntityNodeClass node;
        node.SetName("child_2");
        node.SetTranslation(glm::vec2(-20.0f, -20.0f));
        node.SetSize(glm::vec2(2.0f, 2.0f));
        node.SetScale(glm::vec2(1.0f, 1.0f));
        node.SetRotation(0.0f);
        entity.AddNode(std::move(node));
    }

    {
        game::AnimationTrackClass track;
        track.SetName("test1");
        entity.AddAnimationTrack(track);
    }
    {
        game::AnimationTrackClass track;
        track.SetName("test2");
        entity.AddAnimationTrack(track);
    }

    {
        game::ScriptVar something("something", 123, game::ScriptVar::ReadOnly);
        game::ScriptVar otherthing("other_thing", "jallukola", game::ScriptVar::ReadWrite);
        entity.AddScriptVar(something);
        entity.AddScriptVar(otherthing);
    }

    TEST_REQUIRE(entity.GetNumNodes() == 3);
    TEST_REQUIRE(entity.GetNode(0).GetName() == "root");
    TEST_REQUIRE(entity.GetNode(1).GetName() == "child_1");
    TEST_REQUIRE(entity.GetNode(2).GetName() == "child_2");
    TEST_REQUIRE(entity.FindNodeByName("root"));
    TEST_REQUIRE(entity.FindNodeByName("child_1"));
    TEST_REQUIRE(entity.FindNodeByName("child_2"));
    TEST_REQUIRE(entity.FindNodeByName("foobar") == nullptr);
    TEST_REQUIRE(entity.FindNodeById(entity.GetNode(0).GetId()));
    TEST_REQUIRE(entity.FindNodeById(entity.GetNode(1).GetId()));
    TEST_REQUIRE(entity.FindNodeById("asg") == nullptr);
    TEST_REQUIRE(entity.GetNumTracks() == 2);
    TEST_REQUIRE(entity.FindAnimationTrackByName("test1"));
    TEST_REQUIRE(entity.FindAnimationTrackByName("sdgasg") == nullptr);
    TEST_REQUIRE(entity.GetNumScriptVars() == 2);
    TEST_REQUIRE(entity.GetScriptVar(0).GetName() == "something");
    TEST_REQUIRE(entity.FindScriptVar("foobar") == nullptr);
    TEST_REQUIRE(entity.FindScriptVar("something"));

    // test linking.
    entity.LinkChild(nullptr, entity.FindNodeByName("root"));
    entity.LinkChild(entity.FindNodeByName("root"), entity.FindNodeByName("child_1"));
    entity.LinkChild(entity.FindNodeByName("root"), entity.FindNodeByName("child_2"));
    TEST_REQUIRE(WalkTree(entity) == "root child_1 child_2");

    // serialization
    {
        auto ret = game::EntityClass::FromJson(entity.ToJson());
        TEST_REQUIRE(ret.has_value());
        TEST_REQUIRE(ret->GetName() == "TestEntityClass");
        TEST_REQUIRE(ret->GetNumNodes() == 3);
        TEST_REQUIRE(ret->GetNode(0).GetName() == "root");
        TEST_REQUIRE(ret->GetNode(1).GetName() == "child_1");
        TEST_REQUIRE(ret->GetNode(2).GetName() == "child_2");
        TEST_REQUIRE(ret->GetId() == entity.GetId());
        TEST_REQUIRE(ret->GetHash() == entity.GetHash());
        TEST_REQUIRE(ret->GetNumTracks() == 2);
        TEST_REQUIRE(ret->FindAnimationTrackByName("test1"));
        TEST_REQUIRE(ret->GetNumScriptVars() == 2);
        TEST_REQUIRE(WalkTree(*ret) == "root child_1 child_2");
    }

    // copy construction and assignment
    {
        auto copy(entity);
        TEST_REQUIRE(copy.GetId() == entity.GetId());
        TEST_REQUIRE(copy.GetHash() == entity.GetHash());
        TEST_REQUIRE(copy.GetNumTracks() == 2);
        TEST_REQUIRE(copy.FindAnimationTrackByName("test1"));
        TEST_REQUIRE(WalkTree(copy) == "root child_1 child_2");

        game::EntityClass temp;
        temp = entity;
        TEST_REQUIRE(temp.GetId() == entity.GetId());
        TEST_REQUIRE(temp.GetHash() == entity.GetHash());
        TEST_REQUIRE(temp.GetNumTracks() == 2);
        TEST_REQUIRE(temp.FindAnimationTrackByName("test1"));
        TEST_REQUIRE(WalkTree(temp) == "root child_1 child_2");
    }

    // clone
    {
        auto clone(entity.Clone());
        TEST_REQUIRE(clone.GetNumNodes() == 3);
        TEST_REQUIRE(clone.GetNode(0).GetName() == "root");
        TEST_REQUIRE(clone.GetNode(1).GetName() == "child_1");
        TEST_REQUIRE(clone.GetNode(2).GetName() == "child_2");
        TEST_REQUIRE(clone.GetId() != entity.GetId());
        TEST_REQUIRE(clone.GetHash() != entity.GetHash());
        TEST_REQUIRE(clone.GetNumTracks() == 2);
        TEST_REQUIRE(clone.FindAnimationTrackByName("test1"));
        TEST_REQUIRE(WalkTree(clone) == "root child_1 child_2");
    }

    // remember, the shape is aligned around the position

    // hit testing
    {
        std::vector<game::EntityNodeClass*> hits;
        std::vector<glm::vec2> hitpos;
        entity.CoarseHitTest(0.0f, 0.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.empty());
        TEST_REQUIRE(hitpos.empty());

        entity.CoarseHitTest(6.0f, 6.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.size() == 1);
        TEST_REQUIRE(hitpos.size() == 1);
        TEST_REQUIRE(hits[0]->GetName() == "root");
        TEST_REQUIRE(math::equals(1.0f, hitpos[0].x));
        TEST_REQUIRE(math::equals(1.0f, hitpos[0].y));

        hits.clear();
        hitpos.clear();
        entity.CoarseHitTest(20.0f, 20.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.size() == 1);
        TEST_REQUIRE(hitpos.size() == 1);
        TEST_REQUIRE(hits[0]->GetName() == "child_1");
        TEST_REQUIRE(math::equals(1.0f, hitpos[0].x));
        TEST_REQUIRE(math::equals(1.0f, hitpos[0].y));
    }

    // whole bounding box.
    {
        const auto& box = entity.GetBoundingRect();
        TEST_REQUIRE(math::equals(-11.0f, box.GetX()));
        TEST_REQUIRE(math::equals(-11.0f, box.GetY()));
        TEST_REQUIRE(math::equals(32.0f, box.GetWidth()));
        TEST_REQUIRE(math::equals(32.0f, box.GetHeight()));
    }


    // node bounding box
    {
        const auto* node = entity.FindNodeByName("root");
        const auto& box = entity.FindNodeBoundingRect(node);
        TEST_REQUIRE(math::equals(5.0f, box.GetX()));
        TEST_REQUIRE(math::equals(5.0f, box.GetY()));
        TEST_REQUIRE(math::equals(10.0f, box.GetWidth()));
        TEST_REQUIRE(math::equals(10.0f, box.GetHeight()));
    }
    // node bounding box
    {
        const auto* node = entity.FindNodeByName("child_1");
        const auto box = entity.FindNodeBoundingRect(node);
        TEST_REQUIRE(math::equals(19.0f, box.GetX()));
        TEST_REQUIRE(math::equals(19.0f, box.GetY()));
        TEST_REQUIRE(math::equals(2.0f, box.GetWidth()));
        TEST_REQUIRE(math::equals(2.0f, box.GetHeight()));
    }

    // coordinate mapping
    {
        const auto* node = entity.FindNodeByName("child_1");
        auto vec = entity.MapCoordsFromNodeModel(1.0f , 1.0f , node);
        TEST_REQUIRE(math::equals(20.0f, vec.x));
        TEST_REQUIRE(math::equals(20.0f, vec.y));

        // inverse operation to MapCoordsFromNode
        vec = entity.MapCoordsToNodeModel(20.0f , 20.0f , node);
        TEST_REQUIRE(math::equals(1.0f, vec.x));
        TEST_REQUIRE(math::equals(1.0f, vec.y));
    }

    // test delete node
    {
        TEST_REQUIRE(entity.GetNumNodes() == 3);
        entity.DeleteNode(entity.FindNodeByName("child_2"));
        TEST_REQUIRE(entity.GetNumNodes() == 2);
        entity.DeleteNode(entity.FindNodeByName("root"));
        TEST_REQUIRE(entity.GetNumNodes() == 0);
    }
}


void unit_test_entity_instance()
{
    game::EntityClass klass;
    {
        game::EntityNodeClass node;
        node.SetName("root");
        node.SetTranslation(glm::vec2(10.0f, 10.0f));
        node.SetSize(glm::vec2(10.0f, 10.0f));
        node.SetScale(glm::vec2(1.0f, 1.0f));
        node.SetRotation(0.0f);
        klass.AddNode(std::move(node));
    }
    {
        game::EntityNodeClass node;
        node.SetName("child_1");
        node.SetTranslation(glm::vec2(10.0f, 10.0f));
        node.SetSize(glm::vec2(2.0f, 2.0f));
        node.SetScale(glm::vec2(1.0f, 1.0f));
        node.SetRotation(0.0f);
        klass.AddNode(std::move(node));
    }
    {
        game::EntityNodeClass node;
        node.SetName("child_2");
        node.SetTranslation(glm::vec2(-20.0f, -20.0f));
        node.SetSize(glm::vec2(2.0f, 2.0f));
        node.SetScale(glm::vec2(1.0f, 1.0f));
        node.SetRotation(0.0f);
        klass.AddNode(std::move(node));
    }
    {
        game::EntityNodeClass node;
        node.SetName("child_3");
        node.SetTranslation(glm::vec2(-20.0f, -20.0f));
        node.SetSize(glm::vec2(2.0f, 2.0f));
        node.SetScale(glm::vec2(1.0f, 1.0f));
        node.SetRotation(0.0f);
        klass.AddNode(std::move(node));
    }
    {
        game::ScriptVar foo("foo", 123, game::ScriptVar::ReadWrite);
        game::ScriptVar bar("bar", 1.0f, game::ScriptVar::ReadOnly);
        klass.AddScriptVar(foo);
        klass.AddScriptVar(std::move(bar));
    }

    klass.LinkChild(nullptr, klass.FindNodeByName("root"));
    klass.LinkChild(klass.FindNodeByName("root"), klass.FindNodeByName("child_1"));
    klass.LinkChild(klass.FindNodeByName("root"), klass.FindNodeByName("child_2"));
    klass.LinkChild(klass.FindNodeByName("child_1"), klass.FindNodeByName("child_3"));
    TEST_REQUIRE(WalkTree(klass) == "root child_1 child_3 child_2");


    // create entity instance

    // test initial state.
    game::Entity instance(klass);
    TEST_REQUIRE(instance.GetNumNodes() == 4);
    TEST_REQUIRE(instance.GetNode(0).GetName() == "root");
    TEST_REQUIRE(instance.GetNode(1).GetName() == "child_1");
    TEST_REQUIRE(instance.GetNode(2).GetName() == "child_2");
    TEST_REQUIRE(instance.GetNode(3).GetName() == "child_3");
    TEST_REQUIRE(WalkTree(instance) == "root child_1 child_3 child_2");

    TEST_REQUIRE(instance.FindScriptVar("foo"));
    TEST_REQUIRE(instance.FindScriptVar("bar"));
    TEST_REQUIRE(instance.FindScriptVar("foo")->IsReadOnly() == false);
    TEST_REQUIRE(instance.FindScriptVar("bar")->IsReadOnly() == true);
    instance.FindScriptVar("foo")->SetValue(444);
    TEST_REQUIRE(instance.FindScriptVar("foo")->GetValue<int>() == 444);

    // todo: test more of the instance api
}

void unit_test_entity_clone_track_bug()
{
    // cloning an entity class with animation track requires
    // remapping the node ids.
    game::EntityNodeClass node;
    node.SetName("root");

    game::TransformActuatorClass actuator;
    actuator.SetNodeId(node.GetId());

    game::AnimationTrackClass track;
    track.SetName("test1");
    track.AddActuator(actuator);

    game::EntityClass klass;
    klass.AddNode(std::move(node));
    klass.AddAnimationTrack(track);

    {
        auto clone = klass.Clone();
        const auto& cloned_node = clone.GetNode(0);
        const auto& cloned_track = clone.GetAnimationTrack(0);
        TEST_REQUIRE(cloned_track.GetActuatorClass(0).GetNodeId() == cloned_node.GetId());
    }
}

void unit_test_entity_class_coords()
{
    game::EntityClass entity;
    entity.SetName("test");

    {
        game::EntityNodeClass node;
        node.SetName("node0");
        node.SetSize(glm::vec2(10.0f, 10.0f));
        node.SetScale(glm::vec2(1.0f, 1.0f));
        node.SetRotation(0.0f);
        entity.LinkChild(nullptr,entity.AddNode(std::move(node)));
    }
    {
        game::EntityNodeClass node;
        node.SetName("node1");
        node.SetTranslation(glm::vec2(100.0f, 100.0f));
        node.SetSize(glm::vec2(50.0f, 10.0f));
        node.SetScale(glm::vec2(1.0f, 1.0f));
        node.SetRotation(math::Pi*0.5);
        entity.LinkChild(entity.FindNodeByName("node0"), entity.AddNode(std::move(node)));
    }

    // the hit coordinate is in the *model* space with the
    // top left corner of the model itself being at 0,0
    // and then extending to the width and height of the node's
    // width and height.
    // so anything that falls outside the range of x >= 0 && x <= width
    // and y >= 0 && y <= height is not within the model itself.
    {
        std::vector<game::EntityNodeClass*> hits;
        std::vector<glm::vec2> hitpos;
        entity.CoarseHitTest(0.0f, 0.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.size() == 1);
        TEST_REQUIRE(hitpos.size() == 1);
        TEST_REQUIRE(math::equals(5.0f, hitpos[0].x));
        TEST_REQUIRE(math::equals(5.0f, hitpos[0].y));
    }
    {
        std::vector<game::EntityNodeClass*> hits;
        std::vector<glm::vec2> hitpos;
        entity.CoarseHitTest(-5.0f, -5.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.size() == 1);
        TEST_REQUIRE(hitpos.size() == 1);
        TEST_REQUIRE(math::equals(0.0f, hitpos[0].x));
        TEST_REQUIRE(math::equals(0.0f, hitpos[0].y));
    }
    {
        // expected: outside the box.
        std::vector<game::EntityNodeClass*> hits;
        std::vector<glm::vec2> hitpos;
        entity.CoarseHitTest(-6.0f, -5.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.size() == 0);
    }
    {
        // expected: outside the box
        std::vector<game::EntityNodeClass*> hits;
        std::vector<glm::vec2> hitpos;
        entity.CoarseHitTest(-5.0f, -6.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.size() == 0);
    }
    {
        // expected: outside the box
        std::vector<game::EntityNodeClass*> hits;
        std::vector<glm::vec2> hitpos;
        entity.CoarseHitTest(6.0f, 0.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.size() == 0);
    }
    {
        // expected: outside the box
        std::vector<game::EntityNodeClass*> hits;
        std::vector<glm::vec2> hitpos;
        entity.CoarseHitTest(0.0f, 6.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.size() == 0);
    }

    // node1's transform is relative to node0
    // since they're linked together, remember it's rotated by 90deg
    {
        std::vector<game::EntityNodeClass*> hits;
        std::vector<glm::vec2> hitpos;
        entity.CoarseHitTest(100.0f, 100.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.size() == 1);
        TEST_REQUIRE(hitpos.size() == 1);
        TEST_REQUIRE(math::equals(25.0f, hitpos[0].x));
        TEST_REQUIRE(math::equals(5.0f, hitpos[0].y));
    }
    {
        std::vector<game::EntityNodeClass*> hits;
        std::vector<glm::vec2> hitpos;
        entity.CoarseHitTest(100.0f, 75.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.size() == 1);
        TEST_REQUIRE(hitpos.size() == 1);
        TEST_REQUIRE(math::equals(0.0f, hitpos[0].x));
        TEST_REQUIRE(math::equals(5.0f, hitpos[0].y));
    }
    {
        std::vector<game::EntityNodeClass*> hits;
        std::vector<glm::vec2> hitpos;
        entity.CoarseHitTest(105.0f , 75.0f , &hits , &hitpos);
        TEST_REQUIRE(hits.size() == 1);
        TEST_REQUIRE(hitpos.size() == 1);
        TEST_REQUIRE(math::equals(0.0f , hitpos[0].x));
        TEST_REQUIRE(math::equals(0.0f , hitpos[0].y));
    }
    {
        std::vector<game::EntityNodeClass*> hits;
        std::vector<glm::vec2> hitpos;
        entity.CoarseHitTest(105.0f , 124.0f , &hits , &hitpos);
        TEST_REQUIRE(hits.size() == 1);
        TEST_REQUIRE(hitpos.size() == 1);
        TEST_REQUIRE(math::equals(49.0f , hitpos[0].x));
        TEST_REQUIRE(math::equals(0.0f , hitpos[0].y));
    }

    // map coords to/from entity node's model
    // the coordinates that go in are in the entity coordinate space.
    // the coordinates that come out are relative to the nodes model
    // coordinate space. The model itself has width/size extent in this
    // space thus any results that fall outside x < 0 && x width or
    // y < 0 && y > height are not within the model's extents.
    {
        auto vec = entity.MapCoordsToNodeModel(0.0f, 0.0f, entity.FindNodeByName("node0"));
        TEST_REQUIRE(math::equals(5.0f, vec.x));
        TEST_REQUIRE(math::equals(5.0f, vec.y));
        vec = entity.MapCoordsFromNodeModel(5.0f, 5.0f, entity.FindNodeByName("node0"));
        TEST_REQUIRE(math::equals(0.0f, vec.x));
        TEST_REQUIRE(math::equals(0.0f, vec.y));

        vec = entity.MapCoordsToNodeModel(-5.0f, -5.0f, entity.FindNodeByName("node0"));
        TEST_REQUIRE(math::equals(0.0f, vec.x));
        TEST_REQUIRE(math::equals(0.0f, vec.y));
        vec = entity.MapCoordsFromNodeModel(0.0f, 0.0f, entity.FindNodeByName("node0"));
        TEST_REQUIRE(math::equals(-5.0f, vec.x));
        TEST_REQUIRE(math::equals(-5.0f, vec.y));

        vec = entity.MapCoordsToNodeModel(5.0f, 5.0f, entity.FindNodeByName("node0"));
        TEST_REQUIRE(math::equals(10.0f, vec.x));
        TEST_REQUIRE(math::equals(10.0f, vec.y));
        vec = entity.MapCoordsFromNodeModel(10.0f, 10.0f, entity.FindNodeByName("node0"));
        TEST_REQUIRE(math::equals(5.0f, vec.x));
        TEST_REQUIRE(math::equals(5.0f, vec.y));

        vec = entity.MapCoordsToNodeModel(15.0f, 15.0f, entity.FindNodeByName("node0"));
        TEST_REQUIRE(math::equals(20.0f, vec.x));
        TEST_REQUIRE(math::equals(20.0f, vec.y));
        vec = entity.MapCoordsFromNodeModel(20.0f, 20.0f, entity.FindNodeByName("node0"));
        TEST_REQUIRE(math::equals(15.0f, vec.x));
        TEST_REQUIRE(math::equals(15.0f, vec.y));
    }
    {
        auto vec = entity.MapCoordsToNodeModel(100.0f, 100.0f, entity.FindNodeByName("node1"));
        TEST_REQUIRE(math::equals(25.0f, vec.x));
        TEST_REQUIRE(math::equals(5.0f, vec.y));
        vec = entity.MapCoordsFromNodeModel(25.0f, 5.0f, entity.FindNodeByName("node1"));
        TEST_REQUIRE(math::equals(100.0f, vec.x));
        TEST_REQUIRE(math::equals(100.0f, vec.y));

        vec = entity.MapCoordsToNodeModel(105.0f, 75.0f, entity.FindNodeByName("node1"));
        TEST_REQUIRE(math::equals(0.0f, vec.x));
        TEST_REQUIRE(math::equals(0.0f, vec.y));
    }

}

int test_main(int argc, char* argv[])
{
    unit_test_entity_node();
    unit_test_entity_class();
    unit_test_entity_instance();
    unit_test_entity_clone_track_bug();
    unit_test_entity_class_coords();
    return 0;
}