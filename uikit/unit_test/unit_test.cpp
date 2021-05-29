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
#include <iostream>

#include "base/test_minimal.h"
#include "base/test_float.h"
#include "base/assert.h"
#include "uikit/layout.h"
#include "uikit/painter.h"
#include "uikit/widget.h"
#include "uikit/window.h"

bool operator==(const uik::FSize& lhs, const uik::FSize& rhs)
{
    return real::equals(lhs.GetWidth(), rhs.GetWidth()) &&
           real::equals(lhs.GetHeight(), rhs.GetHeight());
}
bool operator!=(const uik::FSize& lhs, const uik::FSize& rhs)
{
    return !(lhs == rhs);
}

bool operator==(const uik::FPoint& lhs, const uik::FPoint& rhs)
{
    return real::equals(lhs.GetX(), rhs.GetX()) &&
           real::equals(lhs.GetY(), rhs.GetY());
}

bool operator==(const uik::FRect& lhs, const uik::FRect& rhs)
{
    return lhs.GetPosition() == rhs.GetPosition() &&
           lhs.GetSize() == rhs.GetSize();
}

class Painter : public uik::Painter
{
public:
    struct Command {
        std::string name;
        std::string widget;
        std::string text;
        float line_height = 0.0f;
        PaintStruct ps;
    };
    mutable std::vector<Command> cmds;

    void DrawWidgetBackground(const WidgetId& id, const PaintStruct& ps) const override
    {
        Command cmd;
        cmd.name   = "draw-widget-background";
        cmd.widget = id;
        cmd.ps     = ps;
        cmds.push_back(std::move(cmd));
    }
    void DrawWidgetBorder(const WidgetId& id, const PaintStruct& ps) const override
    {
        Command cmd;
        cmd.name   = "draw-widget-border";
        cmd.widget = id;
        cmd.ps     = ps;
        cmds.push_back(std::move(cmd));
    }
    void DrawWidgetText(const WidgetId& id, const PaintStruct& ps, const std::string& text, float line_height) const override
    {
        Command cmd;
        cmd.name   = "draw-widget-text";
        cmd.widget = id;
        cmd.ps     = ps;
        cmd.text   = text;
        cmd.line_height = line_height;
        cmds.push_back(std::move(cmd));
    }
    void DrawFocusRect(const WidgetId& id, const PaintStruct& ps) const override
    {}
    void DrawCheckBox(const WidgetId& id, const PaintStruct& ps, bool checked) const override
    {}
    void DrawButton(const WidgetId& id, const PaintStruct& ps, Button btn) const override
    {}
    void DrawProgressBar(const WidgetId&, const PaintStruct& ps, float percentage) const override
    {}
    bool ParseStyle(const WidgetId& id, const std::string& style) override
    { return true; }
private:
};

class TestWidget : public uik::Widget
{
public:
    struct MouseData {
        std::string name;
        MouseEvent event;
    };
    std::vector<MouseData> mouse;
    std::string name;
    uik::FSize  size;
    uik::FPoint  point;
    base::bitflag<uik::Widget::Flags> flags;
    TestWidget()
    {
        flags.set(uik::Widget::Flags::Enabled, true);
        flags.set(uik::Widget::Flags::VisibleInGame, true);
    }

    virtual std::string GetId() const override
    { return "1234id"; }
    virtual std::string GetName() const override
    { return this->name; }
    virtual std::size_t GetHash() const override
    { return 0x12345; }
    virtual std::string GetStyleString() const override
    { return ""; }
    virtual uik::FSize GetSize() const override
    { return size; }
    virtual uik::FPoint GetPosition() const override
    { return point; }
    virtual Type GetType() const override
    { return TestWidget::Type::Label; }
    virtual bool TestFlag(Flags flag) const override
    { return this->flags.test(flag); }
    virtual void SetName(const std::string& name) override
    { this->name = name; }
    virtual void SetSize(const uik::FSize& size) override
    { this->size = size; }
    virtual void SetPosition(const uik::FPoint& pos) override
    { this->point = pos; }
    virtual void SetStyleString(const std::string& style) override
    {}
    virtual void SetFlag(Flags flag, bool on_off) override
    { this->flags.set(flag, on_off); }
    virtual void IntoJson(nlohmann::json& json) const override
    {}
    virtual bool FromJson(const nlohmann::json& json) override
    { return true; }

    virtual void Paint(const PaintEvent& paint, uik::State& state, uik::Painter& painter) const override
    {}
    virtual Action MouseEnter(uik::State& state) override
    {
        MouseData m;
        m.name = "enter";
        mouse.push_back(m);
        return Action{};
    }
    virtual Action MousePress(const MouseEvent& mouse, uik::State& state) override
    {
        MouseData m;
        m.name = "press";
        m.event = mouse;
        this->mouse.push_back(m);
        return Action{};
    }
    virtual Action MouseRelease(const MouseEvent& mouse, uik::State& state) override
    {
        MouseData m;
        m.name = "release";
        m.event = mouse;
        this->mouse.push_back(m);
        return Action{};
    }
    virtual Action MouseMove(const MouseEvent& mouse, uik::State& state) override
    {
        MouseData m;
        m.name = "move";
        m.event = mouse;
        this->mouse.push_back(m);
        return Action{};
    }
    virtual Action MouseLeave(uik::State& state) override
    {
        MouseData m;
        m.name = "leave";
        mouse.push_back(m);
        return Action{};
    }

    virtual std::unique_ptr<Widget> Copy() const override
    { return std::make_unique<TestWidget>(); }
    virtual std::unique_ptr<Widget> Clone() const override
    { return std::make_unique<TestWidget>(); }
private:
};

template<typename Widget>
void unit_test_widget()
{
    Widget widget;
    TEST_REQUIRE(widget.GetId() != "");
    TEST_REQUIRE(widget.GetName() == "");
    TEST_REQUIRE(widget.GetHash());
    TEST_REQUIRE(widget.GetStyleString() == "");
    TEST_REQUIRE(widget.GetSize() != uik::FSize(0.0f, 0.0f));
    TEST_REQUIRE(widget.GetPosition() == uik::FPoint(0.0f, 0.0f));
    TEST_REQUIRE(widget.TestFlag(uik::Widget::Flags::VisibleInGame));
    TEST_REQUIRE(widget.TestFlag(uik::Widget::Flags::Enabled));
    TEST_REQUIRE(widget.IsEnabled());
    TEST_REQUIRE(widget.IsVisible());

    widget.SetName("widget");
    widget.SetStyleString("style string");
    widget.SetSize(100.0f, 150.0f);
    widget.SetPosition(45.0f, 50.0f);
    widget.SetFlag(uik::Widget::Flags::VisibleInGame, false);

    nlohmann::json json;
    widget.IntoJson(json);
    {
        Widget other;
        TEST_REQUIRE(other.FromJson(json));
        TEST_REQUIRE(other.GetId() == widget.GetId());
        TEST_REQUIRE(other.GetName() == widget.GetName());
        TEST_REQUIRE(other.GetHash() == widget.GetHash());
        TEST_REQUIRE(other.GetStyleString() == widget.GetStyleString());
        TEST_REQUIRE(other.GetSize() == widget.GetSize());
        TEST_REQUIRE(other.GetPosition() == widget.GetPosition());
        TEST_REQUIRE(other.IsEnabled() == widget.IsEnabled());
        TEST_REQUIRE(other.IsVisible() == widget.IsVisible());
    }
}


void unit_test_label()
{
    unit_test_widget<uik::Label>();

    uik::Label widget;
    widget.SetText("hello");
    widget.SetLineHeight(2.0f);
    widget.SetSize(100.0f, 50.0f);

    // widget doesn't respond to mouse presses

    // widget doesn't respond to key presses

    // paint.
    {
        Painter p;
        uik::State s;
        uik::Widget::PaintEvent paint;
        paint.rect = widget.GetRect();
        widget.Paint(paint, s, p);
        TEST_REQUIRE(p.cmds[1].text == "hello");
        TEST_REQUIRE(p.cmds[1].line_height == real::float32(2.0f));
        TEST_REQUIRE(p.cmds[1].ps.rect == uik::FRect(0.0f, 0.0f, 100.0f, 50.0f));
    }
}

void unit_test_pushbutton()
{
    unit_test_widget<uik::PushButton>();

    uik::PushButton widget;
    widget.SetText("OK");
    widget.SetSize(100.0f, 20.0f);

    // mouse press/release -> button press action
    {
        uik::State s;
        uik::Widget::MouseEvent event;
        event.widget_window_rect = widget.GetRect();
        event.window_mouse_pos   = uik::FPoint(10.0f, 10.0f);
        event.button             = uik::MouseButton::Left;

        auto action = widget.MouseEnter(s);
        TEST_REQUIRE(action.type == uik::Widget::ActionType::None);
        action = widget.MousePress(event, s);
        TEST_REQUIRE(action.type == uik::Widget::ActionType::None);
        action = widget.MouseRelease(event, s);
        TEST_REQUIRE(action.type == uik::Widget::ActionType::ButtonPress);
        action = widget.MouseLeave(s);
        TEST_REQUIRE(action.type == uik::Widget::ActionType::None);
    }

    // mouse leaves while button down has occurred -> no action
    {
        uik::State s;
        uik::Widget::MouseEvent event;
        event.widget_window_rect = widget.GetRect();
        event.window_mouse_pos   = uik::FPoint(10.0f, 10.0f);
        event.button             = uik::MouseButton::Left;
        auto action = widget.MouseEnter(s);
        TEST_REQUIRE(action.type == uik::Widget::ActionType::None);
        action = widget.MousePress(event, s);
        TEST_REQUIRE(action.type == uik::Widget::ActionType::None);
        action = widget.MouseLeave(s);
        TEST_REQUIRE(action.type == uik::Widget::ActionType::None);
    }

    // todo: key press testing.


    // paint.
    {
        Painter p;
        uik::State s;
        uik::Widget::PaintEvent paint;
        paint.rect = widget.GetRect();
        widget.Paint(paint, s, p);
        TEST_REQUIRE(p.cmds[1].text == "OK");
        TEST_REQUIRE(p.cmds[1].ps.rect == uik::FRect(0.0f, 0.0f, 100.0f, 20.0f));
        TEST_REQUIRE(p.cmds[1].ps.pressed == false);

        uik::Widget::MouseEvent event;
        event.widget_window_rect = widget.GetRect();
        event.window_mouse_pos   = uik::FPoint(10.0f, 10.0f);
        event.button             = uik::MouseButton::Left;
        widget.MouseEnter(s);
        widget.MousePress(event, s);
        widget.Paint(paint, s, p);
        TEST_REQUIRE(p.cmds[3].ps.pressed == true);
    }
}

void unit_test_checkbox()
{
    unit_test_widget<uik::CheckBox>();

    // todo:
}

void unit_test_window()
{
    uik::Window win;
    TEST_REQUIRE(win.GetId() != "");
    TEST_REQUIRE(win.GetName() == "");
    TEST_REQUIRE(win.GetSize() != uik::FSize());
    TEST_REQUIRE(win.GetHeight() != real::float32(0.0f));
    TEST_REQUIRE(win.GetWidth() != real::float32(0.0f));
    TEST_REQUIRE(win.GetNumWidgets() == 0);
    TEST_REQUIRE(win.FindWidgetByName("foo") == nullptr);
    TEST_REQUIRE(win.FindWidgetById("foo") == nullptr);

    win.SetName("window");
    win.SetStyleName("window_style.json");
    win.Resize(400.0f, 500.0f);

    {
        uik::Label widget;
        widget.SetName("label");
        widget.SetText("label");
        win.AddWidget(widget);

    }

    {
        uik::PushButton widget;
        widget.SetName("pushbutton");
        widget.SetText("pushbutton");
        win.AddWidget(widget);
    }

    // container widget with some contained widgets in order
    // to have some widget recursion
    {
        uik::GroupBox group;
        group.SetName("groupbox");
        group.SetText("groupbox");

        uik::Label label;
        label.SetName("child_label0");
        label.SetText("child label0");
        group.AddWidget(label);
        win.AddWidget(group);
    }

    TEST_REQUIRE(win.GetNumWidgets() == 3);
    TEST_REQUIRE(win.GetWidget(0).GetName() == "label");
    TEST_REQUIRE(win.GetWidget(1).GetName() == "pushbutton");
    TEST_REQUIRE(win.GetWidget(2).GetName() == "groupbox");
    TEST_REQUIRE(win.GetWidget(2).IsContainer());
    TEST_REQUIRE(win.GetWidget(2).GetChild(0).GetName() == "child_label0");
    TEST_REQUIRE(win.FindWidgetByName(win.GetWidget(0).GetName()) == &win.GetWidget(0));
    TEST_REQUIRE(win.FindWidgetById(win.GetWidget(0).GetId()) == &win.GetWidget(0));

    // serialize.
    {
        nlohmann::json json = win.ToJson();
        // for debugging
        //std::cout << json.dump(2);

        auto ret = uik::Window::FromJson(json);
        TEST_REQUIRE(ret.has_value());
        TEST_REQUIRE(ret->GetName() == "window");
        TEST_REQUIRE(ret->GetStyleName() == "window_style.json");
        TEST_REQUIRE(ret->GetNumWidgets() == 3);
        TEST_REQUIRE(ret->GetWidget(0).GetName() == "label");
        TEST_REQUIRE(ret->GetWidget(1).GetName() == "pushbutton");
        TEST_REQUIRE(ret->GetWidget(2).GetName() == "groupbox");
        TEST_REQUIRE(ret->GetWidget(2).IsContainer());
        TEST_REQUIRE(ret->GetWidget(2).GetChild(0).GetName() == "child_label0");
        TEST_REQUIRE(ret->FindWidgetByName(ret->GetWidget(0).GetName()) == &ret->GetWidget(0));
        TEST_REQUIRE(ret->FindWidgetById(ret->GetWidget(0).GetId()) == &ret->GetWidget(0));
        TEST_REQUIRE(ret->GetHash() == win.GetHash());
    }

    // copy
    {
        uik::Window copy(win);
        TEST_REQUIRE(copy.GetName() == "window");
        TEST_REQUIRE(copy.GetStyleName() == "window_style.json");
        TEST_REQUIRE(copy.GetNumWidgets() == 3);
        TEST_REQUIRE(copy.GetWidget(0).GetName() == "label");
        TEST_REQUIRE(copy.GetWidget(1).GetName() == "pushbutton");
        TEST_REQUIRE(copy.GetWidget(2).GetName() == "groupbox");
        TEST_REQUIRE(copy.GetWidget(2).IsContainer());
        TEST_REQUIRE(copy.GetWidget(2).GetChild(0).GetName() == "child_label0");
        TEST_REQUIRE(copy.GetHash() == win.GetHash());
    }

    // assignment
    {
        uik::Window copy;
        copy = win;
        TEST_REQUIRE(copy.GetName() == "window");
        TEST_REQUIRE(copy.GetStyleName() == "window_style.json");
        TEST_REQUIRE(copy.GetNumWidgets() == 3);
        TEST_REQUIRE(copy.GetWidget(0).GetName() == "label");
        TEST_REQUIRE(copy.GetWidget(1).GetName() == "pushbutton");
        TEST_REQUIRE(copy.GetWidget(2).GetName() == "groupbox");
        TEST_REQUIRE(copy.GetWidget(2).IsContainer());
        TEST_REQUIRE(copy.GetWidget(2).GetChild(0).GetName() == "child_label0");
        TEST_REQUIRE(copy.GetHash() == win.GetHash());
    }
}

void unit_test_window_paint()
{
    uik::Window win;
    win.Resize(500.0f, 500.0f);
    {
        uik::PushButton widget;
        widget.SetSize(50.0f, 20.0f);
        widget.SetPosition(25.0f, 35.0f);
        widget.SetName("pushbutton");
        widget.SetText("pushbutton");
        win.AddWidget(widget);
    }
    // container widget with some contained widgets in order
    // to have some widget recursion
    {
        uik::GroupBox group;
        group.SetName("groupbox");
        group.SetText("groupbox");
        group.SetSize(100.0f, 150.0f);
        group.SetPosition(300.0f, 400.0f);

        uik::Label label;
        label.SetName("child_label0");
        label.SetText("child label0");
        label.SetSize(100.0f, 10.0f);
        label.SetPosition(5.0f, 5.0f);
        group.AddWidget(label);

        win.AddWidget(group);

    }
    uik::State state;
    const uik::Widget* form   = win.FindWidgetByName("Form");
    const uik::Widget* button = win.FindWidgetByName("pushbutton");
    const uik::Widget* group  = win.FindWidgetByName("groupbox");
    const uik::Widget* label  = win.FindWidgetByName("child_label0");

    Painter p;
    win.Paint(state, p);

    // form.
    {
        TEST_REQUIRE(p.cmds[0].name == "draw-widget-background");
        TEST_REQUIRE(p.cmds[0].widget == form->GetId());
        TEST_REQUIRE(p.cmds[0].ps.clip == uik::FRect(0.0f, 0.0f, 500.0f, 500.0f));
        TEST_REQUIRE(p.cmds[0].ps.rect == uik::FRect(0.0f, 0.0f, 500.0f, 500.0f));
        TEST_REQUIRE(p.cmds[1].name == "draw-widget-border");
        TEST_REQUIRE(p.cmds[1].widget == form->GetId());
        TEST_REQUIRE(p.cmds[1].ps.clip == uik::FRect(0.0f, 0.0f, 500.0f, 500.0f));
        TEST_REQUIRE(p.cmds[1].ps.rect == uik::FRect(0.0f, 0.0f, 500.0f, 500.0f));
    }

    // push button.
    {
        TEST_REQUIRE(p.cmds[2].name == "draw-widget-background");
        TEST_REQUIRE(p.cmds[2].widget == button->GetId());
        TEST_REQUIRE(p.cmds[2].ps.clip == uik::FRect(0.0f, 0.0f, 500.0f, 500.0f));
        TEST_REQUIRE(p.cmds[2].ps.rect == uik::FRect(25.0f, 35.0f, 50.0f, 20.0f));
        TEST_REQUIRE(p.cmds[3].name == "draw-widget-text");
        TEST_REQUIRE(p.cmds[3].widget == button->GetId());
        TEST_REQUIRE(p.cmds[3].ps.clip == uik::FRect(0.0f, 0.0f, 500.0f, 500.0f));
        TEST_REQUIRE(p.cmds[3].ps.rect == uik::FRect(25.0f, 35.0f, 50.0f, 20.0f));
        TEST_REQUIRE(p.cmds[4].name == "draw-widget-border");
        TEST_REQUIRE(p.cmds[4].widget == button->GetId());
        TEST_REQUIRE(p.cmds[4].ps.clip == uik::FRect(0.0f, 0.0f, 500.0f, 500.0f));
        TEST_REQUIRE(p.cmds[4].ps.rect == uik::FRect(25.0f, 35.0f, 50.0f, 20.0f));
    }

    // group box.
    {
        TEST_REQUIRE(p.cmds[5].name == "draw-widget-background");
        TEST_REQUIRE(p.cmds[5].widget == group->GetId());
        TEST_REQUIRE(p.cmds[5].ps.clip == uik::FRect(0.0f, 0.0f, 500.0f, 500.0f));
        TEST_REQUIRE(p.cmds[5].ps.rect == uik::FRect(300.0f, 400.0f, 100.0f, 150.0f));
        TEST_REQUIRE(p.cmds[6].name == "draw-widget-border");
        TEST_REQUIRE(p.cmds[6].widget == group->GetId());
        TEST_REQUIRE(p.cmds[6].ps.clip == uik::FRect(0.0f, 0.0f, 500.0f, 500.0f));
        TEST_REQUIRE(p.cmds[6].ps.rect == uik::FRect(300.0f, 400.0f, 100.0f, 150.0f));
    }

    // label (child of the group box)
    {
        TEST_REQUIRE(p.cmds[7].name == "draw-widget-background");
        TEST_REQUIRE(p.cmds[7].widget == label->GetId());
        TEST_REQUIRE(p.cmds[7].ps.clip == uik::FRect(300.0f, 400.0f, 100.0f, 150.0f));
        TEST_REQUIRE(p.cmds[7].ps.rect == uik::FRect(305.0f, 405.0f, 100.0f, 10.0f));
    }
}


void unit_test_window_mouse()
{
    uik::Window win;
    win.Resize(500.0f, 500.0f);

    {
        TestWidget t;
        t.SetName("widget0");
        t.SetSize(uik::FSize(40.0f, 40.0f));
        t.SetPosition(uik::FPoint(20.0f, 20.0f));
        win.AddWidget(t);
    }

    {
        TestWidget t;
        t.SetName("widget1");
        t.SetSize(uik::FSize(20.0f, 20.0f));
        t.SetPosition(uik::FPoint(100.0f, 100.0f));
        win.AddWidget(t);
    }

    auto* widget0 = static_cast<TestWidget*>(win.FindWidgetByName("widget0"));
    auto* widget1 = static_cast<TestWidget*>(win.FindWidgetByName("widget1"));

    // mouse enter, mouse leave.
    {
        uik::State state;
        uik::Window::MouseEvent mouse;
        mouse.window_mouse_pos = uik::FPoint(10.0f , 10.0f);
        win.MouseMove(mouse , state);

        mouse.window_mouse_pos = uik::FPoint(25.0f , 25.0f);
        win.MouseMove(mouse , state);
        TEST_REQUIRE(widget0->mouse[0].name == "enter");
        TEST_REQUIRE(widget0->mouse[1].name == "move");
        TEST_REQUIRE(widget0->mouse[1].event.widget_mouse_pos == uik::FPoint(5.0f , 5.0f));

        mouse.window_mouse_pos = uik::FPoint(65.0f, 65.0f);
        win.MouseMove(mouse , state);
        TEST_REQUIRE(widget0->mouse[2].name == "leave");
    }

}

void unit_test_window_transforms()
{
    // todo: hit test, widget rect
}

int test_main(int argc, char* argv[])
{
    unit_test_label();
    unit_test_pushbutton();
    unit_test_checkbox();
    unit_test_window();
    unit_test_window_paint();
    unit_test_window_mouse();

    return 0;
}