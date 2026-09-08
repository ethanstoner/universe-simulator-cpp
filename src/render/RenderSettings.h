#pragma once

namespace render {

// Everything the viewer can change that does not affect the physics. Kept in
// one struct so the UI has a single object to edit and the renderer a single
// object to read.
struct RenderSettings {
    // --- scale -------------------------------------------------------------
    // Metres per world unit. Physics never sees any of this block.
    double metresPerUnit = 1.0;

    // Body radii are exaggerated by a POWER LAW, not a constant multiplier:
    //
    //     drawnRadius = gain * (trueRadius / metresPerUnit) ^ exponent
    //
    // A constant multiplier cannot work at solar-system scale. The Sun is 109
    // Earth radii; any multiplier large enough to make the Earth visible makes
    // the Sun wider than the Earth's entire orbit, which is exactly what the
    // first attempt at this did. An exponent below 1 compresses that ratio
    // while preserving the ordering, so the Sun still reads as much the largest
    // body without eating the scene.
    //
    // This is a display transform and is documented as one. Set trueScale to
    // see the real proportions -- at which point most planets vanish, which is
    // itself worth seeing once.
    bool trueScale = false;
    double bodyVisualGain = 1.0;      // computed per scene, then user-adjustable
    double bodyVisualExponent = 0.35;
    double minVisualRadius = 0.02;    // world units
    double maxVisualRadius = 40.0;    // a backstop, not the usual limiter

    // --- what to draw ------------------------------------------------------
    bool showBodies = true;
    bool showTrails = true;
    bool showGrid = true;
    bool showLabels = true;
    bool showVelocityVectors = false;
    bool showAccelerationVectors = false;
    bool showSchwarzschildRadius = false;
    bool showBoundsBox = false;
    bool wireframe = false;

    // --- trails ------------------------------------------------------------
    float trailOpacity = 0.85f;
    int trailMaxSamples = 1200;

    // --- spacetime grid (a VISUALISATION -- see docs/PHYSICS.md) ------------
    int gridResolution = 140;      // cells per side
    double gridExtent = 60.0;      // half-width, world units
    float gridStrength = 1.0f;     // global multiplier on well depth
    float gridOpacity = 0.42f;
    double gridMaxDepth = 9.0;     // world units; the displacement saturates here
    // Softening radius for each well, as a fraction of gridExtent. Without this
    // the funnel would be a spike one pixel wide.
    float gridWellSoftening = 0.055f;
    bool gridFollowsCamera = true; // recentre the patch under the camera
    bool gridShaded = false;       // draw as a shaded sheet instead of wireframe

    // --- lighting ----------------------------------------------------------
    float ambient = 0.05f;
    float starBrightness = 1.0f;

    // --- mesh quality ------------------------------------------------------
    int sphereLatitudeSegments = 24;
    int sphereLongitudeSegments = 40;
};

}  // namespace render
