# Programmer's Guide: Raw API

WIP

Valid for Pico-SDK / ESP-IDF

From [BTstack documentation][btstack_multithreading]

> BTstack is not thread-safe, but you're using a multi-threading OS.
> Any function that is called from BTstack, e.g., packet handlers, can directly call into BTstack without issues.
> For other situations, you need to provide some general 'do BTstack tasks' function and trigger BTstack to execute
> it on its own thread. To call a function from the BTstack thread, you can use `btstack_run_loop_execute_on_main_thread()`
> allows to directly schedule a function callback, i.e. 'do BTstack tasks' function, from the BTstack thread.
> The called function should check if there are any pending BTstack tasks and execute them.

[btstack_multithreading]: https://github.com/bluekitchen/btstack/blob/master/port/esp32/README.md#multi-threading
