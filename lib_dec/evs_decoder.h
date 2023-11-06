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

#include "options.h"
#include "stl.h"
#include "stat_dec_fx.h"
#include "prot_fx.h"
#include "g192.h"
#include "disclaimer.h"

#include "EvsRXlib.h"

typedef struct DecoderDataBuf{
   char            data[L_FRAME48k];
   int size;
}DecoderDataBuf;

typedef struct EvsDecoderContext
{
	int mode;
	long frame;
	Word16 quietMode;
	Word16 noDelayCmp;
	UWord16           bit_stream[MAX_BITS_PER_FRAME + 16];
	char              *jbmTraceFileName ;           /* VOIP tracefile name         */
	char              *jbmFECoffsetFileName;       /* FEC offset file name */
	FILE *f_stream;                     /*input bitstream file*/
	FILE *f_synth; 						/* output synthesis file     */
	Decoder_State_fx * st_fx;
}EvsDecoderContext;

EvsDecoderContext* NewEvsDecoder();
int InitDecoder(EvsDecoderContext *dec);
int StartDecoder(EvsDecoderContext *dec,const char* data,const int len,DecoderDataBuf* buf);
int StopDecoder(EvsDecoderContext *dec);
int UnitTestEvsDecoder();

#endif