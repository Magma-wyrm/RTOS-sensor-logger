#include "app.h"
#include "bsp.h"

int main(void)
{
    bsp_init();
    app_start();   /* never returns */
    return 0;
}
