#include "Font.hpp"
#include "pango/pangocairo.h"
#include "src/config.hpp"
#include <iostream>

std::list<Font> Font::fonts;

Font* Font::get_font(int px) {
    // Check if the font with the desired pixel height already exists
    for (auto &f : fonts) {
        if (f.height == px)
            return &f;
    }

    auto fontMap = pango_cairo_font_map_get_default();
    auto tempContext = pango_font_map_create_context(fontMap);

    double points = px * (72.0 / config::appearence::dpi); // Initial conversion from pixels to points
    int actualHeight = 0;
    double adjustment = 0.5; // Adjustment step for fine-tuning the font size

    // Set up initial font description
    auto fontDesc = pango_font_description_from_string(config::appearence::font); // Use the global font family variable

    while (true) {
        pango_font_description_set_size(fontDesc, static_cast<int>((points + adjustment) * PANGO_SCALE));

        auto pangoFont = pango_font_map_load_font(fontMap, tempContext, fontDesc);
        auto metrics = pango_font_get_metrics(pangoFont, pango_language_get_default());

        // Calculate the actual pixel height of the font
        int ascent = pango_font_metrics_get_ascent(metrics);
        int descent = pango_font_metrics_get_descent(metrics);
        actualHeight = PANGO_PIXELS(ascent + descent);

        // Check if the actual height matches the requested height
        if (actualHeight == px) {
            break; // Perfect match found
        } else if (actualHeight < px) {
            points += adjustment; // Increase font size if actual height is less than desired
        } else {
            // If overshooting, reduce adjustment and try decreasing
            if (adjustment > 0.1) {
                adjustment /= 2;
                points -= adjustment; // Fine-tune by decreasing font size
            } else {
                break; // Accept the closest match if further adjustment is minimal
            }
        }

        pango_font_metrics_unref(metrics);
        g_object_unref(pangoFont);

        // Prevent infinite loop in case of unexpected behavior
        if (adjustment < 0.01) {
            break;
        }
    }

    // Create and cache the new Font object
    auto description = wl_unique_ptr<PangoFontDescription> {fontDesc};
    auto &res = fonts.emplace_back(Font{std::move(description), actualHeight});

    g_object_unref(tempContext);

    return &res;
}
