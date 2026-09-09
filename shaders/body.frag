#version 330 core

// All positions here are camera-relative (the eye is at the origin), which is
// how the renderer avoids float precision loss at astronomical distances.

in vec3 vViewPosition;
in vec3 vNormal;

const int MAX_LIGHTS = 4;

uniform vec3 uLightPositions[MAX_LIGHTS];
uniform vec3 uLightColors[MAX_LIGHTS];
uniform int uLightCount;

uniform vec3 uBaseColor;
uniform float uEmissive;      // 1 for stars: drawn unlit and glowing
uniform float uAmbient;
uniform float uSelected;      // 1 when this body is the inspector's selection

out vec4 fragColor;

void main() {
    vec3 normal = normalize(vNormal);
    vec3 toEye = normalize(-vViewPosition);

    if (uEmissive > 0.5) {
        // A star is its own light source. The rim term fakes a limb glow so it
        // reads as a sphere rather than a flat disc.
        float rim = pow(1.0 - max(dot(normal, toEye), 0.0), 2.0);
        // Deliberately above 1.0: the HDR target keeps the overflow, the bright
        // pass picks it up, and the tone map rolls the core to white while the
        // bloom halo retains the star's colour.
        vec3 color = uBaseColor * (1.95 + 1.30 * rim);
        fragColor = vec4(color, 1.0);
    } else {
        vec3 lit = uBaseColor * uAmbient;
        for (int i = 0; i < uLightCount && i < MAX_LIGHTS; ++i) {
            vec3 toLight = uLightPositions[i] - vViewPosition;
            float distance = length(toLight);
            if (distance <= 0.0) continue;
            vec3 lightDir = toLight / distance;

            float diffuse = max(dot(normal, lightDir), 0.0);

            // Blinn-Phong specular, kept subtle: planets are not mirrors, but a
            // small highlight makes the sphere's curvature legible.
            vec3 halfway = normalize(lightDir + toEye);
            float specular = pow(max(dot(normal, halfway), 0.0), 48.0) * 0.25;

            // Deliberately NOT inverse-square. Scene scales span four orders of
            // magnitude, so a physical falloff would leave the outer planets
            // black. Lighting is a readability device, not a radiometric model.
            lit += uBaseColor * uLightColors[i] * diffuse + uLightColors[i] * specular;
        }

        // Rim light towards the camera, which separates a planet from the black
        // background when it is backlit.
        float rim = pow(1.0 - max(dot(normal, toEye), 0.0), 3.0) * 0.30;
        lit += uBaseColor * rim;

        if (uSelected > 0.5) lit += vec3(0.16, 0.20, 0.10);

        fragColor = vec4(lit, 1.0);
    }
}
