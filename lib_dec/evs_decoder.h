#ifndef _DLL_EVSDECODER_
#define _DLL_EVSDECODER_

/*====================================================================================
    EVS Codec 3GPP TS26.442 Nov 13, 2018. Version 12.12.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <stdbool.h>
#include "options.h"
#include "stl.h"
#include "stat_dec_fx.h"
#include "prot_fx.h"
#include "g192.h"
#include "disclaimer.h"

#include "EvsRXlib.h"

typedef struct DecoderDataBuf{
   Word16        data[L_FRAME48k];
   short size;
}DecoderDataBuf;

typedef struct EvsDecoderContext
{
	int mode;
	long frame;
	Word16 quietMode;
	Word16 noDelayCmp;
	Word16 dec_delay;
	Word16 zero_pad;
	UWord16           bit_stream[MAX_BITS_PER_FRAME + 16];
	char              *jbmTraceFileName ;           /* VOIP tracefile name         */
	char              *jbmFECoffsetFileName;       /* FEC offset file name */
	FILE *f_stream;                     /*input bitstream file*/
	FILE *f_synth; 						/* output synthesis file     */
	Decoder_State_fx * st_fx;
	DecoderDataBuf* buf;
}EvsDecoderContext;

EvsDecoderContext* NewEvsDecoder();
int InitDecoder(EvsDecoderContext *dec,int sample,int bitRate, int isG192Format);
int EvsStartDecoder(EvsDecoderContext *dec,const char* data,const int len);
int StopDecoder(EvsDecoderContext *dec);
int UnitTestEvsDecoder();

#endif