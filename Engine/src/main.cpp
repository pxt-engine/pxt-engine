#pragma once

#include "application.hpp"

int main() {

    PXTEngine::Logger::init();

    try {
        PXTEngine::Unique<PXTEngine::Application> app(PXTEngine::initApplication());

        app->start();
        app->run();

    } catch (const std::exception& e) {
        PXT_ERROR("Unhandled exception: {}", e.what());
        return EXIT_FAILURE;
	}
    

    return EXIT_SUCCESS;
}