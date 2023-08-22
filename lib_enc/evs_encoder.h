#ifndef _EVS_ENCODER_
#define _EVS_ENCODER_
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <stdbool.h>

#include "options.h"
#include "stl.h"
#include "disclaimer.h"
#include "g192.h"
#include "stat_enc_fx.h"
#include "prot_fx.h"


typedef struct EncoderDataBuf
{
   char data[(2+MAX_BITS_PER_FRAME)*4];
   int size;
}EncoderDataBuf;

typedef struct EvsEncoderContext
{
	long frame; 
	Word16 quietMode;
	Word16 noDelayCmp;
	Indice_fx ind_list[MAX_NUM_INDICES];                  /* list of indices */
	FILE  *f_input;                                         /* input signal file */
    FILE *f_rate;                                         /* bitrate switching profile file */
    FILE *f_bwidth;                                       /* bandwidth switching profile file */
    FILE *f_rf ;                                    /* Channel aware configuration file */
	FILE *f_stream;                     /*output bitstream file*/
	Encoder_State_fx * st_fx;
	EncoderDataBuf* buf;
}EvsEncoderContext;


EvsEncoderContext* NewEvsEncoder();
int InitEncoder(EvsEncoderContext *enc,int sample,int bitRate, char* codec, int isG192Format);//默认设置为MIME
int EvsStartEncoder(EvsEncoderContext *enc,const char* data,const int len);
int StopEncoder(EvsEncoderContext *enc);
int UnitTestEvsEncoder();

#endif