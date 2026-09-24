#ifndef WENA_CHECKLIST_ENTRY_FORM_H
#define WENA_CHECKLIST_ENTRY_FORM_H
#include "../checklist_store.h"
struct nk_context;
/* Shared checklist title/item entry. Caller owns draft, error and save intent.
 * Addition offers a lossless single/multiple-item mode switch. */
unsigned int wena_checklist_entry_form(struct nk_context *context,
    WenaChecklistAction *action,char *input,int *length,int capacity,int *error);
#endif
