// Exercise native layout, range maintenance and wheel/keyboard behavior.
#include "ui/EdgeScrollView.h"
#include <Application.h>
#include <LayoutBuilder.h>
#include <StringItem.h>
#include <Window.h>
#include <cstdio>
#include <cmath>

static int checks = 0, failures = 0;
static void Check(bool ok, const char* description)
{
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", description); }
}
int main()
{
    BApplication app("application/x-vnd.Kambuzi-EdgeScrollTests");
    auto* window = new BWindow(BRect(60, 60, 300, 360), "Scroll test",
        B_TITLED_WINDOW, B_ASYNCHRONOUS_CONTROLS);
    auto* list = new EdgeScrollListView();
    auto* scroll = new EdgeScrollView(list);
    BLayoutBuilder::Group<>(window, B_VERTICAL, 0).Add(scroll);
    for (int i = 0; i < 3; ++i) list->AddItem(new BStringItem("Short list"));
    window->Show();
    snooze(150000);
    window->Lock();
    scroll->DoLayout();
    Check(scroll->ScrollBar(B_VERTICAL)->IsHidden(), "scrollbar chrome hidden");
    Check(std::fabs(list->Frame().Width() - scroll->Bounds().Width()) < 1,
        "list occupies full width without scrollbar gap");
    Check(!list->MoreAbove() && !list->MoreBelow(), "short list has no hints");
    for (int i = 0; i < 60; ++i) list->AddItem(new BStringItem("More contacts"));
    Check(!list->MoreAbove() && list->MoreBelow(), "long list starts with bottom hint only");
    BMessage wheel(B_MOUSE_WHEEL_CHANGED);
    wheel.AddFloat("be:wheel_delta_y", 2);
    wheel.AddFloat("be:wheel_delta_x", 0);
    static_cast<BHandler*>(list)->MessageReceived(&wheel);
    Check(list->Bounds().top > 0, "native wheel scrolls with hidden scrollbar");
    Check(list->MoreAbove() && list->MoreBelow(), "middle has both hints");
    char end = B_END; list->KeyDown(&end, 1);
    Check(list->MoreAbove() && !list->MoreBelow(), "End reaches bottom");
    char home = B_HOME; list->KeyDown(&home, 1);
    Check(!list->MoreAbove() && list->MoreBelow(), "Home reaches top");
    list->ScrollTo(BPoint(0, 100000));
    Check(list->MoreAbove() && !list->MoreBelow(), "scroll range clamps at bottom");
    while (list->CountItems() > 3) delete list->RemoveItem(list->CountItems() - 1);
    Check(!list->MoreAbove() && !list->MoreBelow(), "content shrink removes both hints");
    for (int i = 0; i < 30; ++i) list->AddItem(new BStringItem("Resize list"));
    list->ScrollTo(BPoint(0, 100000));
    window->ResizeTo(400, 740);
    window->Unlock();
    snooze(150000);
    window->Lock();
    Check(std::fabs(list->Frame().Width() - scroll->Bounds().Width()) < 1,
        "resize keeps full width");
    Check(!list->MoreAbove() && !list->MoreBelow(), "enlarged viewport hides hints when contents fit");
    window->Quit();
    std::printf("%d/%d native edge scroll checks passed\n", checks-failures, checks);
    return failures ? 1 : 0;
}
