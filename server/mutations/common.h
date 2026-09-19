#ifndef WENA_MUTATION_COMMON_H
#define WENA_MUTATION_COMMON_H
#include "../domain_operation.h"
/* Shared form decoder used only inside the existing guarded apply transaction.
 * Modes: 0 required single line, 1 multiline including empty, 2 single line
 * including empty. No mode permits NUL, DEL or Unicode C1 controls. */
int wena_mutation_text(const WenaDomainCommand *command,const char *name,
    char *out,size_t capacity,int mode);
int wena_mutation_decimal(const char *text,int allow_zero,unsigned long maximum,
    unsigned long *result);
void wena_mutation_identity(const WenaDomainCommand *command,
    const char *operation,char *id);
#endif
