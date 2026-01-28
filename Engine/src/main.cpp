#include "application.hpp"

#include "core/logging/console_logger_sink.hpp"
#include "core/logging/file_logger_sink.hpp"

int main() {

    // Register logger sinks
    pxt::core::Logger::registerSink(pxt::createShared<pxt::core::ConsoleLoggerSink>());
    pxt::core::Logger::registerSink(pxt::createShared<pxt::core::FileLoggerSink>());

    try {
        pxt::Unique<pxt::Application> app(pxt::initApplication());

        app->run();

    } catch (const std::exception& e) {
        PXT_ERROR("Unhandled exception: {}", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}