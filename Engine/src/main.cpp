#pragma once

#include "application.hpp"

int main() {

    pxt::core::Logger::init();

    try {
        pxt::Unique<pxt::Application> app(pxt::initApplication());

        app->run();

    } catch (const std::exception& e) {
        PXT_ERROR("Unhandled exception: {}", e.what());
        return EXIT_FAILURE;
	}
    

    return EXIT_SUCCESS;
}