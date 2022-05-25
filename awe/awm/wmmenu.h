#pragma once

#include "awe/types.h"
#include <string>
#include <vector>

class Window;
class WindowManager;

using WMFunc = void(*)();
using WMItem = std::pair<std::string, WMFunc>;

class WMMenu final
{
    WindowManager& wm;
    const std::vector<WMItem>& items;
    Window& window;
    int selectedItemIndex = 0;

    void RenderWMWindow();

    awe::Size DetermineSize() const;
    int CalculateWMItemIndex(const awe::Point& pos) const;

public:
    WMMenu(WindowManager& wm, const std::vector<WMItem>& items, const awe::Point& pos);
    ~WMMenu();

    void OnWMWindowMouseMotion(const awe::Point& pos);
    void OnWMMouseLeftButtonDown(const awe::Point& pos);

    Window& GetWindow() const { return window; }

private:
};
