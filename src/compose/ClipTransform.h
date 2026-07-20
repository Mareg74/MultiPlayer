#pragma once

struct ClipTransform
{
    // Normalized crop insets [0..1]
    float cropLeft = 0.f;
    float cropTop = 0.f;
    float cropRight = 0.f;
    float cropBottom = 0.f;

    // Position of clip center in canvas pixels
    float posX = 0.f;
    float posY = 0.f;

    // 1.0 = contain cropped source in the full composition canvas (width)
    float scale = 1.f;
    // Height scale (same as scale for Fit/Fill; independent for Stretch)
    float scaleY = 1.f;
    float opacity = 1.f;
    bool visible = true;
};
