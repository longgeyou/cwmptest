﻿




//#include <global.h>
//#include <md5.h>
//#include <string.h>
//#include "digcalc.h"

//修改
#include "global.h"
#include "md5.h"
#include <string.h>
#include "digcalc.h"



void CvtHex(
    IN HASH Bin,
    OUT HASHHEX Hex
    )
{
    unsigned short i;
    unsigned char j;

    for (i = 0; i < HASHLEN; i++) {
        j = (Bin[i] >> 4) & 0xf;
        if (j <= 9)
            Hex[i*2] = (j + '0');
         else
            Hex[i*2] = (j + 'a' - 10);
        j = Bin[i] & 0xf;
        if (j <= 9)
            Hex[i*2+1] = (j + '0');
         else
            Hex[i*2+1] = (j + 'a' - 10);
    };
    Hex[HASHHEXLEN] = '\0';
};

//参数类型强转，否则有编译警告
#define MD5Update_to(context, input, inputLen)  MD5Update(context, (unsigned char *)(input), inputLen)
#define MD5Final_to(digest, context) MD5Final((unsigned char *)(digest), context)

/* calculate H(A1) as per spec */
void DigestCalcHA1(
    IN char * pszAlg,
    IN char * pszUserName,
    IN char * pszRealm,
    IN char * pszPassword,
    IN char * pszNonce,
    IN char * pszCNonce,
    OUT HASHHEX SessionKey
    )
{
      MD5_CTX Md5Ctx;
      HASH HA1;

      MD5Init(&Md5Ctx);
      MD5Update_to(&Md5Ctx, pszUserName, strlen(pszUserName));
      MD5Update_to(&Md5Ctx, ":", 1);
      MD5Update_to(&Md5Ctx, pszRealm, strlen(pszRealm));
      MD5Update_to(&Md5Ctx, ":", 1);
      MD5Update_to(&Md5Ctx, pszPassword, strlen(pszPassword));
      MD5Final_to(HA1, &Md5Ctx);
      //if (stricmp(pszAlg, "md5-sess") == 0) {   //注，C语言库没有 stricmp 函数， 用 strcasecmp （POSIX 兼容系统）替代 
      if (strcasecmp(pszAlg, "md5-sess") == 0) { 
            MD5Init(&Md5Ctx);
            MD5Update_to(&Md5Ctx, HA1, HASHLEN);
            MD5Update_to(&Md5Ctx, ":", 1);
            MD5Update_to(&Md5Ctx, pszNonce, strlen(pszNonce));
            MD5Update_to(&Md5Ctx, ":", 1);
            MD5Update_to(&Md5Ctx, pszCNonce, strlen(pszCNonce));
            MD5Final_to(HA1, &Md5Ctx);
      };
      CvtHex(HA1, SessionKey);
};

/* calculate request-digest/response-digest as per HTTP Digest spec */
void DigestCalcResponse(
    IN HASHHEX HA1,           /* H(A1) */
    IN char * pszNonce,       /* nonce from server */
    IN char * pszNonceCount,  /* 8 hex digits */
    IN char * pszCNonce,      /* client nonce */
    IN char * pszQop,         /* qop-value: "", "auth", "auth-int" */
    IN char * pszMethod,      /* method from the request */
    IN char * pszDigestUri,   /* requested URL */
    IN HASHHEX HEntity,       /* H(entity body) if qop="auth-int" */
    OUT HASHHEX Response      /* request-digest or response-digest */
    )
{
      MD5_CTX Md5Ctx;
      HASH HA2;
      HASH RespHash;
       HASHHEX HA2Hex;

      // calculate H(A2)
      MD5Init(&Md5Ctx);
      MD5Update_to(&Md5Ctx, pszMethod, strlen(pszMethod));
      MD5Update_to(&Md5Ctx, ":", 1);
      MD5Update_to(&Md5Ctx, pszDigestUri, strlen(pszDigestUri));
      //if (stricmp(pszQop, "auth-int") == 0) {
      if (strcasecmp(pszQop, "auth-int") == 0) {
            MD5Update_to(&Md5Ctx, ":", 1);
            MD5Update_to(&Md5Ctx, HEntity, HASHHEXLEN);
      };
      MD5Final_to(HA2, &Md5Ctx);
       CvtHex(HA2, HA2Hex);

      // calculate response
      MD5Init(&Md5Ctx);
      MD5Update_to(&Md5Ctx, HA1, HASHHEXLEN);
      MD5Update_to(&Md5Ctx, ":", 1);
      MD5Update_to(&Md5Ctx, pszNonce, strlen(pszNonce));
      MD5Update_to(&Md5Ctx, ":", 1);
      if (*pszQop) {
          MD5Update_to(&Md5Ctx, pszNonceCount, strlen(pszNonceCount));
          MD5Update_to(&Md5Ctx, ":", 1);
          MD5Update_to(&Md5Ctx, pszCNonce, strlen(pszCNonce));
          MD5Update_to(&Md5Ctx, ":", 1);
          MD5Update_to(&Md5Ctx, pszQop, strlen(pszQop));
          MD5Update_to(&Md5Ctx, ":", 1);
      };
      MD5Update_to(&Md5Ctx, HA2Hex, HASHHEXLEN);
      MD5Final_to(RespHash, &Md5Ctx);
      CvtHex(RespHash, Response);
};
