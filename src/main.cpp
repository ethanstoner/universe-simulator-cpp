#include <cstdio>
#include <exception>

#include "engine/Application.h"

int main(int argc, char** argv) {
    try {
        const engine::AppOptions options = engine::AppOptions::parse(argc, argv);

        // Exporting configs needs no window, so handle it before one is made.
        // This is how configs/ is regenerated after editing a preset in code.
        if (!options.exportConfigs.empty()) {
            if (!options.noConfigs) engine::loadSceneConfigs("configs");
            const int written = engine::exportSceneConfigs(options.exportConfigs);
            return written > 0 ? 0 : 1;
        }

        engine::Application app(options);
        return app.run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "fatal: %s\n", error.what());
        return 1;
    }
}
