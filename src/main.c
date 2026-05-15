#include "app.h"

#include <stdio.h>

int main(int argc, char **argv)
{
    app_t app;

    int rc = app_init(&app, argc, argv);

    if (rc != 0) {

        fprintf(stderr,
                "startup failed\n");
	/* free memory if init failed  but this can 
	 * can be done only if app_shutdown is safe
	 * even with partial app_init */
        app_shutdown(&app);

        return 1;
    }

    rc = app_run(&app);

    app_shutdown(&app);

    return rc;
}
