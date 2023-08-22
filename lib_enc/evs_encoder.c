#include "evs_encoder.h"

/***************************************************************
Usage: EVS_cod.exe[Options] R Fs input_file bitstream_file

	Mandatory parameters :
	   -------------------- -
		   R : Bitrate in bps,
		   for EVS native modes R = (5900 * , 7200, 8000, 9600, 13200, 16400,
			   24400, 32000, 48000, 64000, 96000, 128000)
			   *VBR mode(average bitrate),
			   for AMR - WB IO modes R = (6600, 8850, 12650, 14250, 15850, 18250,
				   19850, 23050, 23850)
				   Alternatively, R can be a bitrate switching file which consits of R values
				   indicating the bitrate for each frame in bit / s.These values are stored in
				   binary format using 4 bytes per value
				   Fs : Input sampling rate in kHz, Fs = (8, 16, 32 or 48)
				   input_file : Input signal filename
				   bitstream_file : Output bitstream filename

				   Options :
	   --------
		   - q : Quiet mode, no frame counters
		   : default is deactivated
		   - dtx D : Activate DTX mode, D = (0, 3 - 100) is the SID update rate
		   where 0 = adaptive, 3 - 100 = fixed in number of frames,
		   default is deactivated
		   - dtx : Activate DTX mode with a SID update rate of 8 frames
		   - rf p o : Activate channel - aware mode for WB and SWB signal at 13.2kbps,
	   where FEC indicator, p : LO or HI, and FEC offset, o : 2, 3, 5, or 7 in number of frames.
		   Alternatively p and o can be replaced by a rf configuration file with each line
		   contains the values of p and o separated by a space,
		   default is deactivated
		   - max_band B : Activate bandwidth limitation, B = (NB, WB, SWB or FB)
		   alternatively, B can be a text file where each line contains
		   "nb_frames B"
		   - no_delay_cmp : Turn off delay compensation
		   - mime : Mime output bitstream file format
		   The encoder produces TS26.445 Annex.2.6 Mime Storage Format, (not RFC4867 Format)
		   default output bitstream file format is G.192
*************************************************************************************************/

EvsEncoderContext* NewEvsEncoder()
{
    EvsEncoderContext  *enc;   
    if ( (enc = (EvsEncoderContext *) calloc( 1, sizeof(EvsEncoderContext) ) ) == NULL )
    {
        fprintf(stderr, "Can not allocate memory for EvsEncoderContext state structure\n");
        return NULL;
    }  

    return enc;
}

int InitEncoder(EvsEncoderContext *enc, int sample, int bitRate, char* codec, int isG192Format)
{
    fprintf(stdout,"init evs encoder  sample:%d bitRate:%d codec:%s isMimeFormat:%d \n", sample, bitRate, codec, isG192Format);
    if ( (enc->st_fx = (Encoder_State_fx *) calloc( 1, sizeof(Encoder_State_fx) ) ) == NULL )
    {
        fprintf(stderr, "Can not allocate memory for Encoder_State_fx state structure\n");
        return -1;
    }

     if ( (enc->buf = (EncoderDataBuf *) calloc( 1, sizeof(EncoderDataBuf) ) ) == NULL )
    {
        fprintf(stderr, "Can not allocate memory for EncoderDataBuf state structure\n");
        return -1;
    }

    enc->f_rate = NULL;                                        
    enc->f_bwidth = NULL;                                      
    enc->f_rf = NULL;
    enc->quietMode = 0;
    enc-> noDelayCmp = 0;
    enc->frame = 0;
    enc->f_stream = NULL;

    char *strSample;
    if(sample == 8000){
    strSample = "8";
    }else if(sample == 16000){
     strSample = "16";
    }else if(sample == 32000){
     strSample = "32";
    }else if(sample == 48000){
     strSample = "48";
    }else{
     strSample = "8";
    }

	char bitRateParam[64];
	sprintf(bitRateParam, "%d", bitRate);

	if (isG192Format == 0) {
		char *argv[] = {"Evs_cod","-MAX_BAND",codec,"-MIME",bitRateParam,strSample,NULL,enc->f_stream};
		int argc = sizeof(argv) / sizeof(argv[0]);
		io_ini_enc_fx(argc, argv, &enc->f_input, &enc->f_stream, &enc->f_rate, &enc->f_bwidth,
			&enc->f_rf,
			&enc->quietMode, &enc->noDelayCmp, enc->st_fx);
	}
	else {
		char *argv[] = {"Evs_cod","-MAX_BAND",codec,bitRateParam,strSample,NULL,enc->f_stream};
		int argc = sizeof(argv) / sizeof(argv[0]);
		io_ini_enc_fx(argc, argv, &enc->f_input, &enc->f_stream, &enc->f_rate, &enc->f_bwidth,
			&enc->f_rf,
			&enc->quietMode, &enc->noDelayCmp, enc->st_fx);
	}

   enc->st_fx->input_frame_fx = extract_l(Mult_32_16(enc->st_fx->input_Fs_fx , 0x0290));
   enc->st_fx->ind_list_fx = enc->ind_list;

   init_encoder_fx(enc->st_fx);

   return 0;
}

 /*------------------------------------------------------------------------------------------*
     * Loop for every frame of input data
     * - Read the input data
     * - Select the best operating mode
     * - Run the encoder
     * - Write the parameters into output bitstream buf
     *------------------------------------------------------------------------------------------*/
int EvsStartEncoder(EvsEncoderContext *enc,const char* data,const int len)
{
    Word16 input_frame;
    Word16 n_samples;
    Word32 bwidth_profile_cnt = 0;                        /* counter of frames for bandwidth switching profile file */
    Word16 Opt_RF_ON_loc, rf_fec_offset_loc;


    UWord8 pFrame[(MAX_BITS_PER_FRAME + 7) >> 3];
    Word16 pFrame_size = 0;
 
    input_frame = enc->st_fx->input_frame_fx;

    Opt_RF_ON_loc = enc->st_fx->Opt_RF_ON;
    rf_fec_offset_loc = enc->st_fx->rf_fec_offset;

   
    n_samples = (Word16)len;
    /*Encode-a-frame loop start*/

    SUB_WMOPS_INIT("enc");

    if(enc->f_rf != NULL)
    {
        read_next_rfparam_fx(
            &enc->st_fx->rf_fec_offset, &enc->st_fx->rf_fec_indicator, enc->f_rf);
        rf_fec_offset_loc = enc->st_fx->rf_fec_offset;
    }

    if(enc->f_rate != NULL)
    {
        /* read next bitrate from profile file (only if invoked on the cmd line) */
        read_next_brate_fx( &enc->st_fx->total_brate_fx, enc->st_fx->last_total_brate_fx,
                                enc->f_rate, enc->st_fx->input_Fs_fx, &enc->st_fx->Opt_AMR_WB_fx, &enc->st_fx->Opt_SC_VBR_fx, &enc->st_fx->codec_mode );
    }

    if (enc->f_bwidth != NULL)
    {
        /* read next bandwidth from profile file (only if invoked on the cmd line) */
        read_next_bwidth_fx( &enc->st_fx->max_bwidth_fx, enc->f_bwidth, &bwidth_profile_cnt, enc->st_fx->input_Fs_fx );
    }

    if( ( enc->st_fx->Opt_RF_ON && ( L_sub( enc->st_fx->total_brate_fx, ACELP_13k20 ) != 0 ||  L_sub( enc->st_fx->input_Fs_fx, 8000 ) == 0 || enc->st_fx->max_bwidth_fx == NB ) )
            || enc->st_fx->rf_fec_offset == 0 )
    {
        if( L_sub( enc->st_fx->total_brate_fx, ACELP_13k20) == 0 )
        {
            enc->st_fx->codec_mode = MODE1;
            reset_rf_indices(enc->st_fx);
        }
        enc->st_fx->Opt_RF_ON = 0;
        enc->st_fx->rf_fec_offset = 0;

    }

    if( Opt_RF_ON_loc && rf_fec_offset_loc != 0 && L_sub( enc->st_fx->total_brate_fx, ACELP_13k20 ) == 0 && L_sub( enc->st_fx->input_Fs_fx, 8000 ) != 0 && enc->st_fx->max_bwidth_fx != NB )
    {
        enc->st_fx->codec_mode = MODE2;
        if(enc->st_fx->Opt_RF_ON == 0)
        {
            reset_rf_indices(enc->st_fx);
        }
        enc->st_fx->Opt_RF_ON = 1;
        enc->st_fx->rf_fec_offset = rf_fec_offset_loc;
    }

    /* in case of 8kHz sampling rate or when in "max_band NB" mode, limit the total bitrate to 24.40 kbps */
    if ( (L_sub( enc->st_fx->input_Fs_fx, 8000 ) == 0 || (enc->st_fx->max_bwidth_fx == NB)) && L_sub( enc->st_fx->total_brate_fx, ACELP_24k40 ) > 0 )
    {
        enc->st_fx->total_brate_fx = ACELP_24k40;
        enc->st_fx->codec_mode = MODE2;
    }

    /* run the main encoding routine */
    if ( enc->st_fx->Opt_AMR_WB_fx )
    {
        SUB_WMOPS_INIT("amr_wb_enc");
        amr_wb_enc_fx( enc->st_fx, (Word16*)data, n_samples);
        END_SUB_WMOPS;
    }
    else
    {
        SUB_WMOPS_INIT("evs_enc");
        /* EVS encoder*/
        evs_enc_fx( enc->st_fx, (Word16*)data, n_samples);
    }
    /* pack indices into serialized payload format */
    if( enc->st_fx->bitstreamformat == MIME )
    {
        indices_to_serial(enc->st_fx, pFrame, &pFrame_size);
    }

    /* write indices into bitstream file */
    write_indices_fx( enc->st_fx, enc->f_stream, pFrame, pFrame_size );

  
	if (enc->st_fx->bitstreamformat == G192) {
		enc->buf->size = enc->st_fx->outDataLenG192;
		memcpy(enc->buf->data, enc->st_fx->outDataG192, enc->st_fx->outDataLenG192);
	}
	else {
		enc->buf->size = enc->st_fx->outDataLen;
		memcpy(enc->buf->data, enc->st_fx->outData, enc->st_fx->outDataLen);
	}
   

    fflush(stderr);

    enc->frame++;
    if (enc->quietMode == 0)
    {
        fprintf( stdout, "%-8ld\b\b\b\b\b\b\b\b", enc->frame );
    }

    return 0;
}


int StopEncoder(EvsEncoderContext *enc)
{
     /* ----- Encode-a-frame loop end ----- */
     if (enc->quietMode == 0)
      {
         fprintf(stdout, "EVS Encoding of %ld frames finished\n\n", enc->frame);
      }
      else
      {
         fprintf(stdout, "EVS Encoding of %ld frames finished\n\n", enc->frame);
      }

    if(enc->f_stream)
    fclose(enc->f_stream);
   if(enc->f_rate)
    fclose(enc->f_rate);
    if(enc->f_bwidth)
    fclose(enc->f_bwidth);
    if (enc)
    {
      free(enc);
      enc = NULL;
    }

    printf( "EVS StopEncoder  success\n" );

    return 0;
}


int UnitTestEvsEncoder()
{
    EvsEncoderContext * enc = NewEvsEncoder();
    InitEncoder(enc,8000,9600,"NB",0);

   FILE * f_input = fopen("./test8K.pcm","rb");
   if(!f_input)
   {
        return -1;
   }

    int n_samples = 0;
    int input_frame = enc->st_fx->input_frame_fx;
    char data[L_FRAME48k];                              /* Input buffer */
   
    fprintf( stdout, "\n------ Running the encoder ------\n\n" );
    while( (n_samples = (short)fread(data, sizeof(short), input_frame, f_input)) > 0 )
    {
       
        EvsStartEncoder(enc, data, n_samples);
    }
    
    StopEncoder(enc);

    return 0;
}