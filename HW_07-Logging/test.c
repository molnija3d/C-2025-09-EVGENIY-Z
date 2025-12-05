#include "logger.h"
#include <unistd.h>

void deep_function() {
    LOG_ERROR("Something went wrong!", 0);
}

void test_function() {
    LOG_INFO("Starting test...", 0);
    deep_function();
}

int main() {
/*
 * Init debug
 */
    log_init("test_application.log");
    log_set_level(LOG_DEBUG);
    
    /*
     * Usage exapmples
     */
    LOG_DEBUG("Debug message with value: %d", 42);
    LOG_INFO("Application started", 0);
    LOG_WARNING("Low memory: %d%%", 85);
    
    /*
     * Emulation of a function with error
     */
    test_function();
    
    /*
     * Closing log gile
     */
    log_close();
    
    return 0;
}
