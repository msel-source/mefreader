/**********************************************************************************************************************
 
 Copyright 2018, Mayo Foundation, Rochester MN. All rights reserved.
 
 To compile for a 64-bit intel system, linking with the following files is necessary:
 meflib.c, mefrec.c
 
 Usage and modification of this source code is governed by the Apache 2.0 license.
 You may not use this file except in compliance with this License.
 A copy of the Apache 2.0 License may be obtained at http://www.apache.org/licenses/LICENSE-2.0
 
 Thanks to all who acknowledge the Mayo Systems Electrophysiology Laboratory, Rochester, MN
 in academic publications of their work facilitated by this software.
 
 Multiscale Electrophysiology Format (MEF) version 3.0
 Copyright 2018, Mayo Foundation, Rochester MN. All rights reserved.
 Written by Matt Stead, Ben Brinkmann, Jan Cimbalnik, and Dan Crepeau.
 
 *************************************************************************************************************************/


#include "meflib.h"

si4 read_mef_ts_data_by_time(si1 *channel_path, si1 *password, si8 start_time, si8 end_time, si4 *decomp_data);

// base function, should not be called by user directly
si4 read_mef_ts_data(si1 *channel_path, si1 *password, si8 start_time, si8 end_time, si4 times_specified, si4 *decomp_data);

// helper functions
si8 sample_for_uutc_c(si8 uutc, CHANNEL *channel);
si8 uutc_for_sample_c(si8 sample, CHANNEL *channel);
void memset_int(si4 *ptr, si4 value, size_t num);
si4 check_block_crc(ui1* block_hdr_ptr, ui4 max_samps, ui1* total_data_ptr, ui8 total_data_bytes);