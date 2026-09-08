#include <cstdio>
#include <exception>

#include "engine/Application.h"

int main(int argc, char** argv) {
    try {
        const engine::AppOptions options = engine::AppOptions::parse(argc, argv);
        engine::Application app(options);
        return app.run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "fatal: %s\n", error.what());
        return 1;
    }
}
