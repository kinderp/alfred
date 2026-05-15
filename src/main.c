#include "app.h"

#include <stdio.h>

int main(int argc, char **argv)
{
    app_t app;

    int rc = app_init(&app, argc, argv);
    
    if (app_init(&app, argc, argv) != 0) {
        fprintf(stderr, "startup failed\n");
        return 1;
    }

    rc = app_run(&app);

    app_shutdown(&app);

    return rc;
}
