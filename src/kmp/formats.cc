# include <xkomp/xkomp.h>

void
xkomp_task_register_formats_kmp(xkomp_t * xkomp)
{
    xkomp_task_register_formats_kmp_task(xkomp);
    xkomp_task_register_formats_kmp_target(xkomp);
}
