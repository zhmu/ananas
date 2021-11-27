#include "wmmenu.h"

#include "window.h"
#include "windowmanager.h"

namespace
{
    const awe::Colour wmNormalItemColour{ 0, 0, 0};
    const awe::Colour wmSelectedItemColour{ 255, 0, 0 };
    const awe::Colour wmItemTextColour{ 255, 255 };
}

WMMenu::WMMenu(WindowManager& wm, const std::vector<WMItem>& items, const awe::Point& pos)
    : wm(wm)
    , items(items)
    , window(wm.CreateWindow(pos, DetermineSize(), -1))
{
    RenderWMWindow();
}

WMMenu::~WMMenu()
{
    wm.DestroyWindow(window);
}

void WMMenu::OnWMWindowMouseMotion(const awe::Point& pos)
{
    selectedItemIndex = CalculateWMItemIndex(pos);
    RenderWMWindow();

    wm.Invalidate(window.GetClientRectangle());
}

void WMMenu::OnWMMouseLeftButtonDown(const awe::Point& pos)
{
    const auto index = CalculateWMItemIndex(pos);
    if (index < 0 || index >= items.size()) return;

    const auto& item = items[index];
    const auto& fn = item.second;
    fn();
}

void WMMenu::RenderWMWindow()
{
    awe::PixelBuffer pb{ window.shmData, window.clientSize };

    const auto& font = wm.font;
    const auto wmSize = DetermineSize();
    for (int itemIndex = 0; itemIndex < items.size(); ++itemIndex) {
        const awe::Rectangle itemRect{
            { 0, itemIndex * font.GetHeight() },
            { wmSize.width, font.GetHeight() }
        };
        pb.FilledRectangle(itemRect, itemIndex == selectedItemIndex ? wmSelectedItemColour : wmNormalItemColour);
        awe::font::DrawText(pb, wm.font, itemRect.point,wmItemTextColour, items[itemIndex].first);
    }
}

awe::Size WMMenu::DetermineSize() const
{
    return { 200, static_cast<int>(items.size()) * wm.font.GetHeight() };
}

int WMMenu::CalculateWMItemIndex(const awe::Point& pos) const
{
    const auto wmClient = window.GetClientRectangle();
    return (pos.y - wmClient.point.y) / wm.font.GetHeight();
}
