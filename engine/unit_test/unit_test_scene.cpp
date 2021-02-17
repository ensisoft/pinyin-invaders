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
#include "engine/scene.h"
#include "engine/entity.h"

bool operator==(const glm::vec2& lhs, const glm::vec2& rhs)
{
    return real::equals(lhs.x, rhs.x) &&
           real::equals(lhs.y, rhs.y);
}

// build easily comparable representation of the render tree
// by concatenating node names into a string in the order
// of traversal.
std::string WalkTree(const game::SceneClass& scene)
{
    std::string names;
    const auto& tree = scene.GetRenderTree();
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

std::string WalkTree(const game::Scene& scene)
{
    std::string names;
    const auto& tree = scene.GetRenderTree();
    tree.PreOrderTraverseForEach([&names](const auto* entity)  {
        if (entity) {
            names.append(entity->GetName());
            names.append(" ");
        }
    });
    if (!names.empty())
        names.pop_back();
    return names;
}

void unit_test_node()
{
    game::SceneNodeClass node;
    node.SetName("root");
    node.SetTranslation(glm::vec2(150.0f, -150.0f));
    node.SetScale(glm::vec2(4.0f, 5.0f));
    node.SetRotation(1.5f);
    node.SetEntityId("entity");

    // to/from json
    {
        auto ret = game::SceneNodeClass::FromJson(node.ToJson());
        TEST_REQUIRE(ret.has_value());
        TEST_REQUIRE(ret->GetName()         == "root");
        TEST_REQUIRE(ret->GetTranslation()  == glm::vec2(150.0f, -150.0f));
        TEST_REQUIRE(ret->GetScale()        == glm::vec2(4.0f, 5.0f));
        TEST_REQUIRE(ret->GetRotation()     == real::float32(1.5f));
        TEST_REQUIRE(ret->GetEntityId()     == "entity");
        TEST_REQUIRE(ret->GetHash() == node.GetHash());
    }

    // test copy and copy ctor
    {
        auto copy(node);
        TEST_REQUIRE(copy.GetHash() == node.GetHash());
        TEST_REQUIRE(copy.GetId() == node.GetId());
        copy = node;
        TEST_REQUIRE(copy.GetHash() == node.GetHash());
        TEST_REQUIRE(copy.GetId() == node.GetId());
    }

    // test clone
    {
        auto clone = node.Clone();
        TEST_REQUIRE(clone.GetHash() != node.GetHash());
        TEST_REQUIRE(clone.GetId() != node.GetId());
        TEST_REQUIRE(clone.GetName()         == "root");
        TEST_REQUIRE(clone.GetTranslation()  == glm::vec2(150.0f, -150.0f));
        TEST_REQUIRE(clone.GetScale()        == glm::vec2(4.0f, 5.0f));
        TEST_REQUIRE(clone.GetRotation()     == real::float32(1.5f));
        TEST_REQUIRE(clone.GetEntityId()     == "entity");
    }
}

void unit_test_scene_class()
{
    // make a small entity for testing.
    auto entity = std::make_shared<game::EntityClass>();
    {
        game::EntityNodeClass node;
        node.SetName("node");
        node.SetSize(glm::vec2(20.0f, 20.0f));
        entity->LinkChild(nullptr, entity->AddNode(std::move(node)));
    }

    // build-up a test scene with some scene nodes.
    game::SceneClass klass;
    TEST_REQUIRE(klass.GetNumNodes() == 0);

    {
        game::SceneNodeClass node;
        node.SetName("root");
        node.SetEntity(entity);
        node.SetTranslation(glm::vec2(0.0f, 0.0f));
        klass.AddNode(node);
    }
    {
        game::SceneNodeClass node;
        node.SetName("child_1");
        node.SetEntity(entity);
        node.SetTranslation(glm::vec2(100.0f, 100.0f));
        klass.AddNode(node);
    }
    {
        game::SceneNodeClass node;
        node.SetName("child_2");
        node.SetEntity(entity);
        node.SetTranslation(glm::vec2(200.0f, 200.0f));
        klass.AddNode(node);
    }

    {
        game::ScriptVar foo("foo", 123, game::ScriptVar::ReadOnly);
        game::ScriptVar bar("bar", 1.0f, game::ScriptVar::ReadWrite);
        klass.AddScriptVar(foo);
        klass.AddScriptVar(std::move(bar));
    }

    TEST_REQUIRE(klass.GetNumNodes() == 3);
    TEST_REQUIRE(klass.GetNode(0).GetName() == "root");
    TEST_REQUIRE(klass.GetNode(1).GetName() == "child_1");
    TEST_REQUIRE(klass.GetNode(2).GetName() == "child_2");
    TEST_REQUIRE(klass.FindNodeByName("root"));
    TEST_REQUIRE(klass.FindNodeById(klass.GetNode(0).GetId()));
    TEST_REQUIRE(klass.FindNodeById("asgas") == nullptr);
    TEST_REQUIRE(klass.FindNodeByName("foasg") == nullptr);
    TEST_REQUIRE(klass.GetNumScriptVars() == 2);
    TEST_REQUIRE(klass.GetScriptVar(0).GetName() == "foo");
    TEST_REQUIRE(klass.GetScriptVar(1).GetName() == "bar");

    klass.LinkChild(nullptr, klass.FindNodeByName("root"));
    klass.LinkChild(klass.FindNodeByName("root"), klass.FindNodeByName("child_1"));
    klass.LinkChild(klass.FindNodeByName("root"), klass.FindNodeByName("child_2"));
    TEST_REQUIRE(WalkTree(klass) == "root child_1 child_2");

    // to/from json
    {
        auto ret = game::SceneClass::FromJson(klass.ToJson());
        TEST_REQUIRE(ret.has_value());
        TEST_REQUIRE(ret->GetNode(0).GetName() == "root");
        TEST_REQUIRE(ret->GetNode(1).GetName() == "child_1");
        TEST_REQUIRE(ret->GetNode(2).GetName() == "child_2");
        TEST_REQUIRE(ret->FindNodeByName("root"));
        TEST_REQUIRE(ret->FindNodeById(klass.GetNode(0).GetId()));
        TEST_REQUIRE(ret->FindNodeById("asgas") == nullptr);
        TEST_REQUIRE(ret->FindNodeByName("foasg") == nullptr);
        TEST_REQUIRE(ret->GetHash() == klass.GetHash());
        TEST_REQUIRE(ret->GetScriptVar(0).GetName() == "foo");
        TEST_REQUIRE(ret->GetScriptVar(1).GetName() == "bar");
        TEST_REQUIRE(WalkTree(*ret) == "root child_1 child_2");
    }

    // test copy and copy ctor
    {
        auto copy(klass);
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
        TEST_REQUIRE(copy.GetId() == klass.GetId());
        TEST_REQUIRE(WalkTree(copy) == "root child_1 child_2");
        copy = klass;
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
        TEST_REQUIRE(copy.GetId() == klass.GetId());
        TEST_REQUIRE(WalkTree(copy) == "root child_1 child_2");
    }

    // test clone
    {
        auto clone = klass.Clone();
        TEST_REQUIRE(clone.GetHash() != klass.GetHash());
        TEST_REQUIRE(clone.GetId() != klass.GetId());
        TEST_REQUIRE(clone.GetNumNodes() == 3);
        TEST_REQUIRE(clone.GetNode(0).GetName() == "root");
        TEST_REQUIRE(clone.GetNode(1).GetName() == "child_1");
        TEST_REQUIRE(clone.GetNode(2).GetName() == "child_2");
        TEST_REQUIRE(WalkTree(clone) == "root child_1 child_2");
    }

    // test breaking node away from the render tree.
    {
        klass.BreakChild(klass.FindNodeByName("root"), false);
        klass.BreakChild(klass.FindNodeByName("child_1"), false);
        klass.BreakChild(klass.FindNodeByName("child_2"), false);
        TEST_REQUIRE(klass.GetNumNodes() == 3);
        TEST_REQUIRE(klass.GetNode(0).GetName() == "root");
        TEST_REQUIRE(klass.GetNode(1).GetName() == "child_1");
        TEST_REQUIRE(klass.GetNode(2).GetName() == "child_2");
        TEST_REQUIRE(WalkTree(klass) == "");

        klass.LinkChild(nullptr, klass.FindNodeByName("root"));
        klass.LinkChild(klass.FindNodeByName("root"), klass.FindNodeByName("child_1"));
        klass.LinkChild(klass.FindNodeByName("root"), klass.FindNodeByName("child_2"));
        TEST_REQUIRE(WalkTree(klass) == "root child_1 child_2");

    }

    // test duplicate node
    {
        klass.DuplicateNode(klass.FindNodeByName("child_2"));
        TEST_REQUIRE(klass.GetNumNodes() == 4);
        TEST_REQUIRE(klass.GetNode(0).GetName() == "root");
        TEST_REQUIRE(klass.GetNode(1).GetName() == "child_1");
        TEST_REQUIRE(klass.GetNode(2).GetName() == "child_2");
        TEST_REQUIRE(klass.GetNode(3).GetName() == "Copy of child_2");
        klass.GetNode(3).SetName("child_3");
        TEST_REQUIRE(WalkTree(klass) == "root child_1 child_2 child_3");
        klass.ReparentChild(klass.FindNodeByName("child_1"), klass.FindNodeByName("child_3"));
        TEST_REQUIRE(WalkTree(klass) == "root child_1 child_3 child_2");
    }

    // test bounding box
    {
        // todo:
    }

    // test hit testing
    {
        std::vector<game::SceneNodeClass*> hits;
        std::vector<glm::vec2> hitpos;
        klass.CoarseHitTest(50.0f, 50.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.empty());

        klass.CoarseHitTest(0.0f, 0.0f, &hits, &hitpos);
        TEST_REQUIRE(!hits.empty());
        TEST_REQUIRE(hits[0]->GetName() == "root");

        hits.clear();
        hitpos.clear();
        klass.CoarseHitTest(100.0f, 100.0f, &hits, &hitpos);
        TEST_REQUIRE(!hits.empty());
        TEST_REQUIRE(hits[0]->GetName() == "child_1");
    }

    // test coordinate mapping
    {
        const auto* node = klass.FindNodeByName("child_1");
        auto vec = klass.MapCoordsFromNodeModel(0.0f , 0.0f , node);
        TEST_REQUIRE(math::equals(100.0f, vec.x));
        TEST_REQUIRE(math::equals(100.0f, vec.y));

        // inverse operation to MapCoordsFromNode
        vec = klass.MapCoordsToNodeModel(100.0f , 100.0f , node);
        TEST_REQUIRE(math::equals(0.0f, vec.x));
        TEST_REQUIRE(math::equals(0.0f, vec.y));
    }

    // test delete node
    {
        klass.DeleteNode(klass.FindNodeByName(("child_3")));
        TEST_REQUIRE(klass.GetNumNodes() == 3);
        klass.DeleteNode(klass.FindNodeByName("child_1"));
        TEST_REQUIRE(klass.GetNumNodes() == 2);
        TEST_REQUIRE(klass.GetNode(0).GetName() == "root");
        TEST_REQUIRE(klass.GetNode(1).GetName() == "child_2");
    }
}

void unit_test_scene_instance()
{
    auto entity = std::make_shared<game::EntityClass>();

    game::SceneClass klass;
    TEST_REQUIRE(klass.GetNumNodes() == 0);
    {
        game::SceneNodeClass node;
        node.SetName("root");
        node.SetEntity(entity);
        klass.AddNode(node);
    }
    {
        game::SceneNodeClass node;
        node.SetName("child_1");
        node.SetEntity(entity);
        klass.AddNode(node);
    }
    {
        game::SceneNodeClass node;
        node.SetName("child_2");
        node.SetEntity(entity);
        klass.AddNode(node);
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

    // the scene instance has the initial state based on the scene class object.
    // i.e. the initial entities are created based on the scene class nodes and
    // their properties.
    game::Scene instance(klass);
    TEST_REQUIRE(instance.GetNumEntities() == 3);
    TEST_REQUIRE(instance.GetEntity(0).GetName() == "root");
    TEST_REQUIRE(instance.GetEntity(1).GetName() == "child_1");
    TEST_REQUIRE(instance.GetEntity(2).GetName() == "child_2");
    TEST_REQUIRE(instance.GetEntity(0).GetId() == klass.GetNode(0).GetId());
    TEST_REQUIRE(instance.GetEntity(1).GetId() == klass.GetNode(1).GetId());
    TEST_REQUIRE(instance.GetEntity(2).GetId() == klass.GetNode(2).GetId());
    TEST_REQUIRE(instance.FindEntityByInstanceName("root"));
    TEST_REQUIRE(instance.FindEntityByInstanceName("blaal") == nullptr);
    TEST_REQUIRE(instance.FindEntityByInstanceId(klass.GetNode(0).GetId()));
    TEST_REQUIRE(instance.FindEntityByInstanceId("asegsa") == nullptr);
    TEST_REQUIRE(WalkTree(instance) == "root child_1 child_2");
    TEST_REQUIRE(instance.FindScriptVar("foo"));
    TEST_REQUIRE(instance.FindScriptVar("bar"));
    TEST_REQUIRE(instance.FindScriptVar("foo")->IsReadOnly() == false);
    TEST_REQUIRE(instance.FindScriptVar("bar")->IsReadOnly() == true);
    instance.FindScriptVar("foo")->SetValue(444);
    TEST_REQUIRE(instance.FindScriptVar("foo")->GetValue<int>() == 444);
    instance.DeleteEntity(instance.FindEntityByInstanceName("child_1"));
    TEST_REQUIRE(instance.GetNumEntities() == 2);
    instance.DeleteEntity(instance.FindEntityByInstanceName("root"));
    TEST_REQUIRE(instance.GetNumEntities() == 0);

    // spawn an entity.
    {
        game::EntityArgs args;
        args.name = "foo";
        args.klass = entity;
        args.id = "13412sfgf12";
        instance.SpawnEntity(args , true /*link to root*/);
        TEST_REQUIRE(instance.GetNumEntities() == 1);
        TEST_REQUIRE(instance.GetEntity(0).GetName() == "foo");
        TEST_REQUIRE(instance.GetEntity(0).GetId() == "13412sfgf12");
        TEST_REQUIRE(WalkTree(instance) == "foo");
        instance.DeleteEntity(instance.FindEntityByInstanceName("foo"));
        TEST_REQUIRE(instance.GetNumEntities() == 0);
    }

    // kill entity at lifetime
    {
        entity->SetFlag(game::EntityClass::Flags::LimitLifetime, true);
        entity->SetFlag(game::EntityClass::Flags::KillAtLifetime, true);
        entity->SetLifetime(2.5);
        game::EntityArgs args;
        args.name = "foo";
        args.klass = entity;
        args.id = "13412sfgf12";
        instance.SpawnEntity(args , true /*link to root*/);
        instance.Update(1.0);
        TEST_REQUIRE(instance.GetEntity(0).HasExpired() == false);
        TEST_REQUIRE(instance.GetEntity(0).HasBeenKilled() == false);
        instance.Update(1.51);
        TEST_REQUIRE(instance.GetEntity(0).HasExpired() == true);
        TEST_REQUIRE(instance.GetEntity(0).HasBeenKilled() == true);
        instance.PruneEntities();
        TEST_REQUIRE(instance.GetNumEntities() == 0);
    }
}

void unit_test_scene_instance_transform()
{
    auto entity0 = std::make_shared<game::EntityClass>();
    {
        game::EntityNodeClass parent;
        parent.SetName("parent");
        parent.SetSize(glm::vec2(10.0f, 10.0f));
        parent.SetTranslation(glm::vec2(0.0f, 0.0f));
        entity0->LinkChild(nullptr,  entity0->AddNode(parent));

        game::EntityNodeClass child0;
        child0.SetName("child0");
        child0.SetSize(glm::vec2(16.0f, 6.0f));
        child0.SetTranslation(glm::vec2(20.0f, 20.0f));
        entity0->LinkChild(entity0->FindNodeByName("parent"), entity0->AddNode(child0));
    }
    auto entity1 = std::make_shared<game::EntityClass>();
    {
        game::EntityNodeClass node;
        node.SetName("node");
        node.SetSize(glm::vec2(5.0f, 5.0f));
        node.SetTranslation(glm::vec2(15.0f, 15.0f));
        entity1->LinkChild(nullptr,  entity1->AddNode(node));
    }

    game::SceneClass klass;
    // setup a scene with 2 entities where the second entity
    // is  linked to one of the nodes in the first entity
    {
        game::SceneNodeClass node;
        node.SetName("entity0");
        node.SetEntity(entity0);
        node.SetTranslation(glm::vec2(-10.0f, -10.0f));
        klass.LinkChild(nullptr, klass.AddNode(node));
    }
    {
        game::SceneNodeClass node;
        node.SetName("entity1");
        node.SetEntity(entity1);
        // link this sucker to so that the nodes in entity1 are transformed relative
        // to child0 node in entity0
        node.SetParentRenderTreeNodeId(entity0->FindNodeByName("child0")->GetId());
        node.SetTranslation(glm::vec2(50.0f, 50.0f));
        klass.LinkChild(klass.FindNodeByName("entity0"), klass.AddNode(node));
    }

    auto scene = game::CreateSceneInstance(klass);

    // check entity nodes.
    // when the scene instance is created the scene nodes
    // are used to give the initial placement of entity nodes in the scene.
    {
        auto* entity0 = scene->FindEntityByInstanceName("entity0");
        auto box = scene->FindEntityNodeBoundingBox(entity0, entity0->FindNodeByInstanceName("parent"));
        TEST_REQUIRE(box.GetSize() == glm::vec2(10.0f, 10.0f));
        TEST_REQUIRE(box.GetTopLeft() == glm::vec2(-10.0f, -10.0f) // placement
                                       + glm::vec2(0.0f, 0.0f) // node's offset relative to entity root
                                       + glm::vec2(-5.0f, -5.0f)); // half the size for model offset

        auto rect = scene->FindEntityNodeBoundingRect(entity0, entity0->FindNodeByInstanceName("parent"));
        TEST_REQUIRE(rect.GetWidth() == real::float32(10.0f));
        TEST_REQUIRE(rect.GetWidth() == real::float32(10.0f));
        TEST_REQUIRE(rect.GetX() == real::float32(-10.0f + 0.0f -5.0f));
        TEST_REQUIRE(rect.GetY() == real::float32(-10.0f + 0.0f -5.0f));

        box = scene->FindEntityNodeBoundingBox(entity0, entity0->FindNodeByInstanceName("child0"));
        TEST_REQUIRE(box.GetSize() == glm::vec2(16.0f, 6.0f));
        TEST_REQUIRE(box.GetTopLeft() == glm::vec2(-10.0f, -10.0f) // entity placement
                                       + glm::vec2(0.0f, 0.0f) // parent offset relative to the entity root
                                       + glm::vec2(20.0f, 20.0f) // node's offset relative to parent
                                       + glm::vec2(-8.0f, -3.0f)); // half the size for model offset
        rect = scene->FindEntityNodeBoundingRect(entity0, entity0->FindNodeByInstanceName("child0"));
        TEST_REQUIRE(rect.GetWidth() == real::float32(16.0f));
        TEST_REQUIRE(rect.GetHeight() == real::float32(6.0f));
        TEST_REQUIRE(rect.GetX() == real::float32(-10.0f + 0.0f + 20.0f -8.0f));
        TEST_REQUIRE(rect.GetY() == real::float32(-10.0f + 0.0f + 20.0f -3.0f));

        // combined bounding rect for both nodes in entity0
        rect = scene->FindEntityBoundingRect(entity0);
        TEST_REQUIRE(rect.GetWidth() == real::float32(15.0 + 18.0f));
        TEST_REQUIRE(rect.GetHeight() == real::float32(15.0 + 13.0f));
        TEST_REQUIRE(rect.GetX() == real::float32(-15.0));
        TEST_REQUIRE(rect.GetY() == real::float32(-15.0));

        auto* entity1 = scene->FindEntityByInstanceName("entity1");
        box = scene->FindEntityNodeBoundingBox(entity1, entity1->FindNodeByInstanceName("node"));
        TEST_REQUIRE(box.GetSize() == glm::vec2(5.0f, 5.0f));
        TEST_REQUIRE(box.GetTopLeft() == glm::vec2(-10.0f, -10.0f) // parent entity placement
                                            + glm::vec2(0.0f, 0.0f)     // parent entity parent node offset relative to entity root
                                            + glm::vec2(20.0f, 20.0f)   // child node offset relative to its entity parent node
                                            + glm::vec2(50.0f, 50.0f)  // this entity placement
                                            + glm::vec2(15.0f, 15.0f) // node placement relative to entity root
                                            + glm::vec2(-2.5, -2.5f)); // half the size for model offset
        // todo: bounding rects for entity1
    }


    {
        // entity0 is linked to the root of the scene graph, therefore the scene graph transform
        // for the nodes in entity0 is identity.
        auto* entity = scene->FindEntityByInstanceName("entity0");
        auto mat = scene->FindEntityTransform(entity);
        game::FBox box(mat);
        TEST_REQUIRE(box.GetWidth() == real::float32(1.0f));
        TEST_REQUIRE(box.GetHeight() == real::float32(1.0f));
        TEST_REQUIRE(box.GetTopLeft() == glm::vec2(0.0f , 0.0f));
        // when the scene instance is created the scene class nodes are used to give the
        // initial placement of entities and the scene class nodes' transforms are
        // baked into the transforms of the top level entity nodes.
        auto* node  = entity->FindNodeByInstanceName("parent");
        box.Reset();
        box.Transform(node->GetModelTransform());
        box.Transform(entity->FindNodeTransform(node));
        TEST_REQUIRE(box.GetWidth() == real::float32(10.0f));
        TEST_REQUIRE(box.GetHeight() == real::float32(10.0f));
        TEST_REQUIRE(box.GetTopLeft() == glm::vec2(-15.0f, -15.0f));

        // 'child0' node's transform is relative to 'parent' node.
        node = entity->FindNodeByInstanceName("child0");
        box.Reset();
        box.Transform(node->GetModelTransform());
        box.Transform(entity->FindNodeTransform(node));
        TEST_REQUIRE(box.GetWidth() == real::float32(16.0f));
        TEST_REQUIRE(box.GetHeight() == real::float32(6.0f));
        TEST_REQUIRE(box.GetTopLeft() == glm::vec2(-10.0, -10.0f) +
                                              glm::vec2(20.0f, 20.0f)  -
                                              glm::vec2(8.0, 3.0));
    }

    {
        // entity1 is linked to entity0 with the link node being child0 in entity0.
        // that means that the nodes in entity1 have a transform that is relative
        // child0 node in entity0.
        auto* entity = scene->FindEntityByInstanceName("entity1");
        auto mat = scene->FindEntityTransform(entity);
        game::FBox box(mat);
        TEST_REQUIRE(box.GetWidth() == real::float32(1.0f));
        TEST_REQUIRE(box.GetHeight() == real::float32(1.0f));
        TEST_REQUIRE(box.GetTopLeft() == glm::vec2(-10.0f , -10.0f) // initial placement
                                         + glm::vec2(20.0f, 20.0f));  // link node offset

        // when the scene instance is created the scene class nodes are used to give the
        // initial placement of entities and the scene class nodes' transforms are
        // baked into the transforms of the top level entity nodes.
        auto* node = entity->FindNodeByInstanceName("node");
        box.Reset();
        box.Transform(node->GetModelTransform());
        box.Transform(entity->FindNodeTransform(node));
        box.Transform(mat);
        TEST_REQUIRE(box.GetWidth() == real::float32(5.0f));
        TEST_REQUIRE(box.GetHeight() == real::float32(5.0f));
        TEST_REQUIRE(box.GetTopLeft() == glm::vec2(-10.0f, -10.0f) // parent entity placement translate
                                         + glm::vec2(0.0f, 0.0f) // parent entity parent node translate
                                         + glm::vec2(20.0f, 20.0f) // parent entity child node translate
                                         + glm::vec2(50.0f, 50.0f) // this entity placement translate
                                         + glm::vec2(15.0f, 15.0f) // this entity node translate
                                         + glm::vec2(-2.5f, -2.5f)); // half model size translate offset
    }

}

int test_main(int argc, char* argv[])
{
    unit_test_node();
    unit_test_scene_class();
    unit_test_scene_instance();
    unit_test_scene_instance_transform();
    return 0;
}