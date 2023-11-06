#include "evs_decoder.h"

/************************************************************
Usage : EVS_dec.exe[Options] Fs bitstream_file output_file

	Mandatory parameters :
		-------------------- -
			Fs : Output sampling rate in kHz(8, 16, 32 or 48)
			bitstream_file : Input bitstream filename or RTP packet filename(in VOIP mode)
			output_file : Output speech filename

			Options :
		--------
			- VOIP : VOIP mode,
			-Tracefile TF : Generate trace file named TF,
			-no_delay_cmp : Turn off delay compensation
			- fec_cfg_file : Optimal channel aware configuration computed by the JBM
			as described in Section 6.3.1 of TS26.448.The output is
			written into a.txt file.Each line contains the FER indicator
			(HI | LO) and optimal FEC offset.
			- mime : Mime input bitstream file format
			The decoder reads both TS26.445 Annex.2.6 and RFC4867 Mime Storage Format,
			the magic word in the mime input file is used to determine the format.
			default input bitstream file format is G.192
			- q : Quiet mode, no frame counter
			default is OFF
******************************************************************************************/

EvsDecoderContext* NewEvsDecoder()
{
    EvsDecoderContext  *dec;   
    if ( (dec = (EvsDecoderContext *) calloc( 1, sizeof(EvsDecoderContext) ) ) == NULL )
    {
        fprintf(stderr, "Can not allocate memory for EvsDecoderContext state structure\n");
        return NULL;
    }  

    return dec; 
}

int InitDecoder(EvsDecoderContext* dec)
{
    if ( (dec->st_fx = (Decoder_State_fx *) calloc(1, sizeof(Decoder_State_fx) ) ) == NULL )
    {
        fprintf(stderr, "Can not allocate memory for Decoder_State_fx state structure\n");
        return -1;
    }

    BASOP_init

    dec->f_stream = NULL;
    dec->f_synth = NULL;
    dec->jbmFECoffsetFileName = NULL;
    dec->jbmTraceFileName = NULL;
    dec->quietMode = 0;
    dec-> noDelayCmp = 0;
    dec->frame  = 0;

    int argc  = 5;
    char *argv[] = {"EVS_dec.exe","-MIME","8",NULL,NULL};

    
    dec->st_fx->bit_stream_fx = dec->bit_stream;
    dec->st_fx->total_brate_fx = 9600;


    io_ini_dec_fx( argc, argv, &dec->f_stream, &dec->f_synth,
                   &dec->quietMode,
                   &dec->noDelayCmp,
                   dec->st_fx,
#ifdef SUPPORT_JBM_TRACEFILE
                   &dec->jbmTraceFileName,
#endif
                   &dec->jbmFECoffsetFileName
                 );

    dec->st_fx->output_frame_fx = extract_l(Mult_32_16(dec->st_fx->output_Fs_fx , 0x0290));

    srand((unsigned int)time(0));

    reset_indices_dec_fx(dec->st_fx);
    
    init_decoder_fx(dec->st_fx);

    return 0;
    
}

/*------------------------------------------------------------------------------------------*
    * Loop for every packet (frame) of bitstream data
    * - Read the bitstream packet
    * - Run the decoder
    * - Write the synthesized signal into output buf
    *------------------------------------------------------------------------------------------*/
int StartDecoder(EvsDecoderContext *dec,const char* data,const int len,DecoderDataBuf* buf)
{
    Word16            zero_pad, dec_delay,output_frame;

    /*output_frame = (short)(st_fx->output_Fs / 50);*/
   
    {
        /*------------------------------------------------------------------------------------------*
         * Regular EVS decoder with ITU-T G.192 bitstream
         *------------------------------------------------------------------------------------------*/

        /* output frame length */
        output_frame = dec->st_fx->output_frame_fx;

        if( dec->noDelayCmp == 0)
        {
            /* calculate the compensation (decoded signal aligned with original signal) */
            /* the number of first output samples will be reduced by this amount */
            dec_delay = NS2SA_fx2(dec->st_fx->output_Fs_fx, get_delay_fx(DEC, dec->st_fx->output_Fs_fx));
        }
        else
        {
            dec_delay = 0;
        }

        zero_pad = dec_delay;

        /*------------------------------------------------------------------------------------------*
         * Loop for every packet (frame) of bitstream data
         * - Read the bitstream packet
         * - Run the decoder
         * - Write the synthesized signal into output file
         *------------------------------------------------------------------------------------------*/
        if (dec->quietMode == 0)
        {
            fprintf( stdout, "\n------ Running the decoder ------\n\n" );
            fprintf( stdout, "Frames processed:       " );
        }
        else {
            fprintf( stdout, "\n-- Start the decoder (quiet mode) --\n\n" );
        }
        BASOP_end_noprint;
        BASOP_init;
#if (WMOPS)
        Init_WMOPS_counter();
        Reset_WMOPS_counter();
        setFrameRate(48000, 960);
#endif

        /*----- loop: decode-a-frame -----*/
        if( dec->st_fx->bitstreamformat==G192 ? read_indices_fx_real( dec->st_fx,(UWord16*)data,(Word16)len, 0 ) : read_indices_mime_real( dec->st_fx, (UWord8*)data, (Word16)len, 0) )
        {
#if (WMOPS)
            fwc();
            Reset_WMOPS_counter();
#endif

            SUB_WMOPS_INIT("evs_dec");
            /* run the main encoding routine */
            if(sub(dec->st_fx->codec_mode, MODE1) == 0)
            {
                if ( dec->st_fx->Opt_AMR_WB_fx )
                {
                    amr_wb_dec_fx( (Word16*)buf->data,dec->st_fx);
                }
                else
                {
                    evs_dec_fx( dec->st_fx, (Word16*)buf->data, FRAMEMODE_NORMAL);
                }
            }
            else
            {
                if(dec->st_fx->bfi_fx == 0)
                {
                    evs_dec_fx( dec->st_fx, (Word16*)buf->data, FRAMEMODE_NORMAL);
                }
                else /* conceal */
                {
                    evs_dec_fx( dec->st_fx, (Word16*)buf->data, FRAMEMODE_MISSING);
                }
            }

            END_SUB_WMOPS;


            /* increase the counter of initialization frames */

            if( sub(dec->st_fx->ini_frame_fx,MAX_FRAME_COUNTER) < 0 )
            {
                dec->st_fx->ini_frame_fx = add(dec->st_fx->ini_frame_fx,1);
            }

          
            dec->frame++;
            if (dec->quietMode == 0)
            {
                fprintf( stdout, "%-8ld\b\b\b\b\b\b\b\b", dec->frame);
            }
        }

        /*----- decode-a-frame-loop end -----*/

        fflush( stderr );
        if (dec->quietMode == 0)
        {
            fprintf( stdout, "\n\n" );
            printf("Decoding finished\n\n");
        }
        else
        {
            printf("Decoding of %ld frames finished\n\n", dec->frame);
        }
        fprintf( stdout, "\n\n" );
        fflush(stdout);



        fflush(stdout);
        fflush(stderr);

        /* end of WMOPS counting */
#if (WMOPS)
        fwc();
        printf("\nDecoder complexity\n");
        WMOPS_output(0);
        printf("\n");
#endif

        /* add zeros at the end to have equal length of synthesized signals */
       // set16_fx( output, 0, zero_pad );
        //fwrite( output, sizeof(Word16), zero_pad, dec->f_synth );
  
    }

    return 0;
}

      
int StopDecoder(EvsDecoderContext *dec)
{
 
    /* free memory etc. */
    if(dec->st_fx)
    {
        printf("destory it decoder...\n");
        destroy_decoder( dec->st_fx );
    }
    if(dec->f_synth)
      fclose( dec->f_synth );
    if(dec->f_stream)
      fclose( dec->f_stream );
   if(dec)
        free(dec);

    return 0;
}

int UnitTestEvsDecoder()
{
   printf("UnitTest EVSEncoder  start...\n");
  
   EvsDecoderContext *dec = NewEvsDecoder();

   FILE * f_input = fopen("./out1.192","rb");
   if(!f_input)
   {
        return -1;
   }

    InitDecoder(dec);
    int n_samples = 0;
    char data[L_FRAME48k];                              /* Input buffer */
    DecoderDataBuf buf;
    fprintf( stdout, "\n------ Running the decoder ------\n\n" );
    while( (n_samples = (short)fread(data, sizeof(unsigned short), 1, f_input)) > 0 )
    {
       
        StartDecoder(dec, data, n_samples,&buf);
    }

   
    StopDecoder(dec);
    return 0;
}