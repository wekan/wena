/* SPDX-License-Identifier: MIT */
#include "sha256.h"

#include <string.h>

typedef WenaShaU32 U32;
typedef WenaSha256 Sha;
#define R(x,n) ((((x) >> (n)) | ((x) << (32u-(n)))) & 0xfffffffful)
#define S0(x) (R(x,2)^R(x,13)^R(x,22))
#define S1(x) (R(x,6)^R(x,11)^R(x,25))
#define G0(x) (R(x,7)^R(x,18)^((x)>>3))
#define G1(x) (R(x,17)^R(x,19)^((x)>>10))

static const U32 k[64] = {
0x428a2f98ul,0x71374491ul,0xb5c0fbcful,0xe9b5dba5ul,0x3956c25bul,0x59f111f1ul,0x923f82a4ul,0xab1c5ed5ul,
0xd807aa98ul,0x12835b01ul,0x243185beul,0x550c7dc3ul,0x72be5d74ul,0x80deb1feul,0x9bdc06a7ul,0xc19bf174ul,
0xe49b69c1ul,0xefbe4786ul,0x0fc19dc6ul,0x240ca1ccul,0x2de92c6ful,0x4a7484aaul,0x5cb0a9dcul,0x76f988daul,
0x983e5152ul,0xa831c66dul,0xb00327c8ul,0xbf597fc7ul,0xc6e00bf3ul,0xd5a79147ul,0x06ca6351ul,0x14292967ul,
0x27b70a85ul,0x2e1b2138ul,0x4d2c6dfcul,0x53380d13ul,0x650a7354ul,0x766a0abbul,0x81c2c92eul,0x92722c85ul,
0xa2bfe8a1ul,0xa81a664bul,0xc24b8b70ul,0xc76c51a3ul,0xd192e819ul,0xd6990624ul,0xf40e3585ul,0x106aa070ul,
0x19a4c116ul,0x1e376c08ul,0x2748774cul,0x34b0bcb5ul,0x391c0cb3ul,0x4ed8aa4aul,0x5b9cca4ful,0x682e6ff3ul,
0x748f82eeul,0x78a5636ful,0x84c87814ul,0x8cc70208ul,0x90befffaul,0xa4506cebul,0xbef9a3f7ul,0xc67178f2ul};

static void transform(Sha *s)
{
    U32 w[64],a,b,c,d,e,f,g,h,t1,t2;
    size_t i;
    for(i=0;i<16;i++) w[i]=((U32)s->block[i*4]<<24)|((U32)s->block[i*4+1]<<16)|((U32)s->block[i*4+2]<<8)|s->block[i*4+3];
    for(i=16;i<64;i++) w[i]=(G1(w[i-2])+w[i-7]+G0(w[i-15])+w[i-16])&0xfffffffful;
    a=s->h[0];b=s->h[1];c=s->h[2];d=s->h[3];e=s->h[4];f=s->h[5];g=s->h[6];h=s->h[7];
    for(i=0;i<64;i++){t1=(h+S1(e)+((e&f)^((~e)&g))+k[i]+w[i])&0xfffffffful;t2=(S0(a)+((a&b)^(a&c)^(b&c)))&0xfffffffful;h=g;g=f;f=e;e=(d+t1)&0xfffffffful;d=c;c=b;b=a;a=(t1+t2)&0xfffffffful;}
    s->h[0]=(s->h[0]+a)&0xfffffffful;s->h[1]=(s->h[1]+b)&0xfffffffful;s->h[2]=(s->h[2]+c)&0xfffffffful;s->h[3]=(s->h[3]+d)&0xfffffffful;
    s->h[4]=(s->h[4]+e)&0xfffffffful;s->h[5]=(s->h[5]+f)&0xfffffffful;s->h[6]=(s->h[6]+g)&0xfffffffful;s->h[7]=(s->h[7]+h)&0xfffffffful;
}

void wena_sha256_update(WenaSha256 *s,const unsigned char *p,size_t n)
{
    size_t take;
    U32 old=s->lo;s->lo=(s->lo+(U32)n)&0xfffffffful;if(s->lo<old)s->hi++;
    while(n){take=64-s->used;if(take>n)take=n;memcpy(s->block+s->used,p,take);s->used+=take;p+=take;n-=take;if(s->used==64){transform(s);s->used=0;}}
}

void wena_sha256_init(WenaSha256 *s)
{
    memset(s,0,sizeof(*s));s->h[0]=0x6a09e667ul;s->h[1]=0xbb67ae85ul;s->h[2]=0x3c6ef372ul;s->h[3]=0xa54ff53aul;s->h[4]=0x510e527ful;s->h[5]=0x9b05688cul;s->h[6]=0x1f83d9abul;s->h[7]=0x5be0cd19ul;
}

void wena_sha256_final_hex(WenaSha256 *s,char output[65])
{
    unsigned char tail[128]; size_t pad,i; U32 bits_hi,bits_lo; static const char x[]="0123456789abcdef";
    bits_hi=((s->hi<<3)|(s->lo>>29))&0xfffffffful;bits_lo=(s->lo<<3)&0xfffffffful;pad=s->used<56?64-s->used:128-s->used;memset(tail,0,pad);tail[0]=0x80;
    tail[pad-8]=(unsigned char)(bits_hi>>24);tail[pad-7]=(unsigned char)(bits_hi>>16);tail[pad-6]=(unsigned char)(bits_hi>>8);tail[pad-5]=(unsigned char)bits_hi;tail[pad-4]=(unsigned char)(bits_lo>>24);tail[pad-3]=(unsigned char)(bits_lo>>16);tail[pad-2]=(unsigned char)(bits_lo>>8);tail[pad-1]=(unsigned char)bits_lo;wena_sha256_update(s,tail,pad);
    for(i=0;i<32;i++){unsigned char v=(unsigned char)(s->h[i/4]>>(24-(i%4)*8));output[i*2]=x[v>>4];output[i*2+1]=x[v&15];}output[64]='\0';
}

void wena_sha256_hex(const unsigned char *data, size_t length, char output[65])
{
    WenaSha256 state;wena_sha256_init(&state);wena_sha256_update(&state,data,length);wena_sha256_final_hex(&state,output);
}
