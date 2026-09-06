#ifndef WENA_SERVER_EXECUTABLE_PATH_H
#define WENA_SERVER_EXECUTABLE_PATH_H
#include <stddef.h>
#define WENA_EXECUTABLE_PATH_CAPACITY 4096u
typedef enum WenaExecutablePlatform{WENA_EXEC_LINUX,WENA_EXEC_BSD,WENA_EXEC_WINDOWS,WENA_EXEC_APPLE,WENA_EXEC_AMIGA,WENA_EXEC_AROS}WenaExecutablePlatform;
typedef int (*WenaExecutablePathQuery)(void*,char*,size_t,size_t*);
int wena_executable_path_validate(WenaExecutablePlatform,WenaExecutablePathQuery,void*,char*,size_t);
int wena_executable_path_current(char*,size_t);
#endif
