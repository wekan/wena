#include "position_input.h"
#include <nuklear.h>
#include <limits.h>
int wena_position_input(struct nk_context *context,int selected,int count,int enabled)
{
    int displayed;
    if (!context) return selected;
    if (!enabled || count<=0 || count==INT_MAX || selected<0 || selected>=count) {
        nk_label(context,"",NK_TEXT_LEFT);return selected;
    }
    displayed=selected+1;
    (void)nk_property_int(context,"#",1,&displayed,count,1,1.0f);
    return displayed>=1 && displayed<=count ? displayed-1 : selected;
}
