#define _POSIX_C_SOURCE 200809L
#include "executable_path.h"
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__) || defined(__unix__)
#include <unistd.h>
#if defined(__FreeBSD__) || defined(__NetBSD__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif
#endif
#if defined(__FreeBSD__) || defined(__NetBSD__)
static int bsd_query(void*x,char*out,size_t cap,size_t*n){size_t z=cap;int mib[4];(void)x;mib[0]=CTL_KERN;
#if defined(__FreeBSD__)
mib[1]=KERN_PROC;mib[2]=KERN_PROC_PATHNAME;mib[3]=-1;if(sysctl(mib,4,out,&z,NULL,0)!=0)return 0;
#else
mib[1]=KERN_PROC_ARGS;mib[2]=-1;mib[3]=KERN_PROC_PATHNAME;if(sysctl(mib,4,out,&z,NULL,0)!=0)return 0;
#endif
if(z==0||z>cap)return 0;out[cap-1]=0;*n=strlen(out);return *n+1==z;}
#endif
static int utf8(const char*s,size_t n){size_t i=0;while(i<n){unsigned char c=(unsigned char)s[i];size_t k;if(c==0||c<0x20)return 0;if(c<0x80){i++;continue;}if(c>=0xc2&&c<=0xdf)k=1;else if(c>=0xe0&&c<=0xef)k=2;else if(c>=0xf0&&c<=0xf4)k=3;else return 0;if(i+k>=n)return 0;while(k){if(((unsigned char)s[i+k]&0xc0)!=0x80)return 0;k--;}i+=(c<0xe0?2:c<0xf0?3:4);}return 1;}
static int absolute(WenaExecutablePlatform p,const char*s){if(p==WENA_EXEC_WINDOWS)return ((s[0]>='A'&&s[0]<='Z')||(s[0]>='a'&&s[0]<='z'))&&s[1]==':'&&(s[2]=='\\'||s[2]=='/');if(p==WENA_EXEC_AMIGA||p==WENA_EXEC_AROS)return strchr(s,':')!=NULL;return s[0]=='/';}
int wena_executable_path_validate(WenaExecutablePlatform p,WenaExecutablePathQuery q,void*x,char*out,size_t cap){size_t n;if(!q||!out||cap<2||cap>WENA_EXECUTABLE_PATH_CAPACITY)return 0;out[0]=0;if(!q(x,out,cap,&n)||n==0||n>=cap||out[n]!=0||strlen(out)!=n||!utf8(out,n)||!absolute(p,out)){out[0]=0;return 0;}return 1;}
#if defined(__linux__) || defined(__unix__)
static int posix_query(void*x,char*out,size_t cap,size_t*n){const char*path=(const char*)x;long z;z=readlink(path,out,cap-1);if(z<=0||(size_t)z>=cap-1)return 0;out[z]=0;*n=(size_t)z;return 1;}
#endif
int wena_executable_path_current(char*out,size_t cap){
#if defined(_WIN32)
wchar_t w[WENA_EXECUTABLE_PATH_CAPACITY];DWORD n=GetModuleFileNameW(NULL,w,WENA_EXECUTABLE_PATH_CAPACITY);int z;if(n==0||n>=WENA_EXECUTABLE_PATH_CAPACITY)return 0;z=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w,(int)n,out,(int)cap-1,NULL,NULL);if(z<=0||(size_t)z>=cap)return 0;out[z]=0;return 1;
#elif defined(__APPLE__)
uint32_t n=(uint32_t)cap;if(_NSGetExecutablePath(out,&n)!=0||n==0||n>=cap){if(cap)out[0]=0;return 0;}return out[0]=='/';
#elif defined(__linux__)
return wena_executable_path_validate(WENA_EXEC_LINUX,posix_query,(void*)"/proc/self/exe",out,cap);
#elif defined(__FreeBSD__) || defined(__NetBSD__)
return wena_executable_path_validate(WENA_EXEC_BSD,bsd_query,NULL,out,cap);
#elif defined(__OpenBSD__)
return wena_executable_path_validate(WENA_EXEC_BSD,posix_query,(void*)"/proc/curproc/file",out,cap);
#else
if(out&&cap)out[0]=0;return 0;
#endif
}
