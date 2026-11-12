// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include "ActionIconButton.h"
#include <Catalog.h>

class SidebarButton final : public ActionIconButton {
public:
    SidebarButton() : ActionIconButton("sidebar-toggle", B_TRANSLATE_CONTEXT("Hide sidebar", "SidebarButton"),
        new BMessage('sbar'), "sidebar", 48, 52) {}
    void SetCollapsed(bool collapsed) {
        SetActionIcon(collapsed ? "sidebar-expand" : "sidebar");
        SetLabel(collapsed ? B_TRANSLATE_CONTEXT("Show sidebar", "SidebarButton")
            : B_TRANSLATE_CONTEXT("Hide sidebar", "SidebarButton"));
        SetToolTip(Label());
    }
};
