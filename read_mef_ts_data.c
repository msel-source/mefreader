/**********************************************************************************************************************
 
 Copyright 2020, Mayo Foundation, Rochester MN. All rights reserved.
 
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
 
 usage notes: it is the user's responsibilitiy to ensure an adequate length decomp_data (sample buffer) is allocated,
 prior to calling this function.
 
 *************************************************************************************************************************/

#include "read_mef_ts_data.h"

// global
extern MEF_GLOBALS	*MEF_globals;

// user specifies a time range
si4 read_mef_ts_data_by_time(si1 *channel_path, si1 *password, si8 start_time, si8 end_time, si4 *decomp_data, CHANNEL *channel_passed_in)
{
    return read_mef_ts_data(channel_path, password, start_time, end_time, 1, decomp_data, channel_passed_in);
}

// user specifies a sample range
si4 read_mef_ts_data_by_samp(si1 *channel_path, si1 *password, si8 start_samp, si8 end_samp, si4 *decomp_data, CHANNEL *channel_passed_in)
{
    return read_mef_ts_data(channel_path, password, start_samp, end_samp, 0, decomp_data, channel_passed_in);
}

// this function should not be called directly by user, but rather by a function specified above
si4 read_mef_ts_data(si1 *channel_path, si1 *password, si8 start_value, si8 end_value, si4 times_specified, si4 *decomp_data, CHANNEL *channel_passed_in)
{
    // Specified by user
    si8     start_time, end_time;
    si8     start_samp, end_samp;
    
    // Method specific variables
    si4     i, j;
    ui4 n_segments;
    CHANNEL    *channel;
    si4 start_segment, end_segment;
    si8  total_samps;//, samp_counter_base;
    ui8  total_data_bytes;
    ui8 start_idx, end_idx, num_blocks;
    ui1 *compressed_data_buffer, *cdp;
    si8  segment_start_sample, segment_end_sample;
    si8  segment_start_time, segment_end_time;
    si8  block_start_time;
    si4 num_block_in_segment;
    FILE *fp;
    ui8 n_read, bytes_to_read;
    RED_PROCESSING_STRUCT   *rps;
    si4 sample_counter;
    ui4 max_samps;
    si4 *temp_data_buf;
    si4 num_samps;
    si4 offset_into_output_buffer;
    si8 block_start_time_offset;
    si4 read_channel;
    
    // check if no buffer is passed in
    if (decomp_data == NULL)
    {
        printf("No sample buffer was passed to function, exiting...");
        return 0;
    }
    
    if (channel_passed_in == NULL)
    {
        read_channel = 1;
        
        // set up mef 3 library
        (void) initialize_meflib();
        MEF_globals->behavior_on_fail = RETURN_ON_FAIL;
        
        channel = read_MEF_channel(NULL, channel_path, TIME_SERIES_CHANNEL_TYPE, password, NULL, MEF_FALSE, MEF_FALSE);
        
        if (channel->channel_type != TIME_SERIES_CHANNEL_TYPE) {
            printf("Not a time series channel, exiting...");
            return 0;
        }
    }
    else
    {
        read_channel = 0;
        channel = channel_passed_in;
    }
    
    // interpret parameters based on whether times or samples are being specified
    
    if (times_specified)
    {
        start_time = start_value;
        end_time = end_value;
    }
    else
    {
        start_samp = start_value;
        end_samp = end_value;
    }
    
    // If None was passed as one of the arguments move to the start or end of the recording
    //start_samp = 0;
    //start_time = channel->earliest_start_time;
    //end_samp = channel->metadata.time_series_section_2->number_of_samples;
    //end_time = channel->latest_end_time;
    
    
    // check if valid data range
    if (times_specified && start_time >= end_time)
    {
        printf("Start time later than end time, exiting...");
        if (read_channel == 1)
        {
            if (channel->number_of_segments > 0)
                channel->segments[0].metadata_fps->directives.free_password_data = MEF_TRUE;
            free_channel(channel, MEF_TRUE);
        }
        return 0;
    }
    if (!times_specified && start_samp >= end_samp)
    {
        printf("Start sample larger than end sample, exiting...");
        if (read_channel == 1)
        {
            if (channel->number_of_segments > 0)
                channel->segments[0].metadata_fps->directives.free_password_data = MEF_TRUE;
            free_channel(channel, MEF_TRUE);
        }
        return 0;
    }
    
    // Fire a warnings if start or stop or both are out of file
    if (times_specified){
        if (((start_time < channel->earliest_start_time) & (end_time < channel->earliest_start_time)) |
            ((start_time > channel->latest_end_time) & (end_time > channel->latest_end_time))){
            printf("Start and stop times are out of file.");
            if (read_channel == 1)
            {
                if (channel->number_of_segments > 0)
                    channel->segments[0].metadata_fps->directives.free_password_data = MEF_TRUE;
                free_channel(channel, MEF_TRUE);
            }
			return 0;
        }
        if (end_time > channel->latest_end_time)
            printf("Stop uutc later than latest end time. Will insert NaNs");
        
        if (start_time < channel->earliest_start_time)
            printf("Start uutc earlier than earliest start time. Will insert NaNs");
    }else{
        if (((start_samp < 0) & (end_samp < 0)) |
            ((start_samp > channel->metadata.time_series_section_2->number_of_samples) & (end_samp > channel->metadata.time_series_section_2->number_of_samples))){
            printf("Start and stop samples are out of file. Returning None");
            if (read_channel == 1)
            {
                if (channel->number_of_segments > 0)
                    channel->segments[0].metadata_fps->directives.free_password_data = MEF_TRUE;
                free_channel(channel, MEF_TRUE);
            }
            return 0;
        }
        if (end_samp > channel->metadata.time_series_section_2->number_of_samples){
            printf("Stop sample larger than number of samples. Setting end sample to number of samples in channel");
            end_samp = channel->metadata.time_series_section_2->number_of_samples;
        }
        if (start_samp < 0){
            printf("Start sample smaller than 0. Setting start sample to 0");
            start_samp = 0;
        }
    }
    // Determine the number of samples
    num_samps = 0;
    if (times_specified)
        num_samps = (si4)(((end_time - start_time) / 1000000.0) * channel->metadata.time_series_section_2->sampling_frequency);
    else
        num_samps = end_samp - start_samp;
    
    //fprintf(stderr, "Num_samps = %d\n", num_samps);
    
    // Iterate through segments, looking for data that matches our criteria
    n_segments = channel->number_of_segments;
    start_segment = end_segment = -1;
    
    // fill in whatever data we don't know
    if (times_specified) {
        start_samp = sample_for_uutc_c(start_time, channel);
        end_samp = sample_for_uutc_c(end_time, channel);
    }else{
        start_time = uutc_for_sample_c(start_samp, channel);
        end_time = uutc_for_sample_c(end_samp, channel);
    }
    
    //fprintf(stderr, "start_time = %ld\n", start_time);
    //fprintf(stderr, "end_time = %ld\n", end_time);
    
    // Find start and stop segments by uutc time
    // find start segment by finding first segment whose ending is past the start time.
    // then find stop segment by using the previous segment of the (first segment whose start is past the end time)
    for (i = 0; i < n_segments; ++i) {
        if (times_specified){
            segment_start_time = channel->segments[i].time_series_data_fps->universal_header->start_time;
            segment_end_time   = channel->segments[i].time_series_data_fps->universal_header->end_time;
            remove_recording_time_offset( &segment_start_time);
            remove_recording_time_offset( &segment_end_time);
            
            //fprintf(stderr, "considering segment: %d\n", i);
            //fprintf(stderr, "segment_start_time: %ld\n", segment_start_time);
            //fprintf(stderr, "segment_end_time: %ld\n", segment_end_time);
            
            if ((segment_end_time >= start_time) && (start_segment == -1)) {
                start_segment = i;
                end_segment = i;
                //fprintf(stderr, "Found\n");
            }
            if ((end_segment != -1) && (segment_start_time <= end_time))
            {
                //fprintf(stderr, "Found2\n");
                end_segment = i;
            }
            
        }else{
            segment_start_sample = channel->segments[i].metadata_fps->metadata.time_series_section_2->start_sample;
            segment_end_sample   = channel->segments[i].metadata_fps->metadata.time_series_section_2->start_sample +
            channel->segments[i].metadata_fps->metadata.time_series_section_2->number_of_samples;
            
            if ((start_samp >= segment_start_sample) && (start_samp <= segment_end_sample))
                start_segment = i;
            if ((end_samp >= segment_start_sample) && (end_samp <= segment_end_sample))
                end_segment = i;
            
        }
    }
    
    //fprintf(stderr, "start_segment = %d\n", start_segment);
    //fprintf(stderr, "end_segment = %d\n", end_segment);
    
    // find start block in start segment
    start_idx = end_idx = 0;
    //samp_counter_base = channel->segments[start_segment].metadata_fps->metadata.time_series_section_2->start_sample;
    for (j = 1; j < channel->segments[start_segment].metadata_fps->metadata.time_series_section_2->number_of_blocks; j++) {
        
        block_start_time = channel->segments[start_segment].time_series_indices_fps->time_series_indices[j].start_time;
        remove_recording_time_offset( &block_start_time);
        
        if (block_start_time > start_time) {
            start_idx = j - 1;
            break;
        }
        // starting point is in last block in segment
        start_idx = j;
    }
    
    // find stop block in stop segment
    //samp_counter_base = channel->segments[end_segment].metadata_fps->metadata.time_series_section_2->start_sample;
    for (j = 1; j < channel->segments[end_segment].metadata_fps->metadata.time_series_section_2->number_of_blocks; j++) {
        
        block_start_time = channel->segments[end_segment].time_series_indices_fps->time_series_indices[j].start_time;
        remove_recording_time_offset( &block_start_time);
        
        if (block_start_time > end_time) {
            end_idx = j - 1;
            break;
        }
        // ending point is in last block in segment
        end_idx = j;
    }
    
    // find total_samps and total_data_bytes, so we can allocate buffers
    total_samps = 0;
    total_data_bytes = 0;
    
    // normal case - everything is in one segment
    if (start_segment == end_segment) {
        if (end_idx < (channel->segments[start_segment].metadata_fps->metadata.time_series_section_2->number_of_blocks - 1)) {
            total_samps += channel->segments[start_segment].time_series_indices_fps->time_series_indices[end_idx+1].start_sample -
            channel->segments[start_segment].time_series_indices_fps->time_series_indices[start_idx].start_sample;
            total_data_bytes += channel->segments[start_segment].time_series_indices_fps->time_series_indices[end_idx+1].file_offset -
            channel->segments[start_segment].time_series_indices_fps->time_series_indices[start_idx].file_offset;
        }
        else {
            // case where end_idx is last block in segment
            total_samps += channel->segments[start_segment].metadata_fps->metadata.time_series_section_2->number_of_samples -
            channel->segments[start_segment].time_series_indices_fps->time_series_indices[start_idx].start_sample;
            total_data_bytes += channel->segments[start_segment].time_series_data_fps->file_length -
            channel->segments[start_segment].time_series_indices_fps->time_series_indices[start_idx].file_offset;
        }
        num_blocks = end_idx - start_idx + 1;
    }
    // spans across segments
    else {
        // start with first segment
        num_block_in_segment = channel->segments[start_segment].metadata_fps->metadata.time_series_section_2->number_of_blocks;
        total_samps += channel->segments[start_segment].metadata_fps->metadata.time_series_section_2->number_of_samples -
        channel->segments[start_segment].time_series_indices_fps->time_series_indices[start_idx].start_sample;
        total_data_bytes +=  channel->segments[start_segment].time_series_data_fps->file_length -
        channel->segments[start_segment].time_series_indices_fps->time_series_indices[start_idx].file_offset;
        num_blocks = num_block_in_segment - start_idx;
        
        if (channel->segments[start_segment].time_series_indices_fps->time_series_indices[start_idx].file_offset < 1024){
            printf("Invalid index file offset, exiting...");
            if (read_channel == 1)
            {
                if (channel->number_of_segments > 0)
                    channel->segments[0].metadata_fps->directives.free_password_data = MEF_TRUE;
                free_channel(channel, MEF_TRUE);
            }
            return 0;
        }
        
        // this loop will only run if there are segments in between the start and stop segments
        for (i = (start_segment + 1); i <= (end_segment - 1); i++) {
            num_block_in_segment = channel->segments[i].metadata_fps->metadata.time_series_section_2->number_of_blocks;
            total_samps += channel->segments[i].metadata_fps->metadata.time_series_section_2->number_of_samples;
            total_data_bytes += channel->segments[i].time_series_data_fps->file_length -
            channel->segments[i].time_series_indices_fps->time_series_indices[0].file_offset;
            num_blocks += num_block_in_segment;
            
            if (channel->segments[i].time_series_indices_fps->time_series_indices[0].file_offset < 1024){
                printf("Invalid index file offset, exiting...");
                if (read_channel == 1)
                {
                    if (channel->number_of_segments > 0)
                        channel->segments[0].metadata_fps->directives.free_password_data = MEF_TRUE;
                    free_channel(channel, MEF_TRUE);
                }
                return 0;
            }
        }
        
        // then last segment
        num_block_in_segment = channel->segments[end_segment].metadata_fps->metadata.time_series_section_2->number_of_blocks;
        if (end_idx < (channel->segments[end_segment].metadata_fps->metadata.time_series_section_2->number_of_blocks - 1)) {
            total_samps += channel->segments[end_segment].time_series_indices_fps->time_series_indices[end_idx+1].start_sample -
            channel->segments[end_segment].time_series_indices_fps->time_series_indices[0].start_sample;
            total_data_bytes += channel->segments[end_segment].time_series_indices_fps->time_series_indices[end_idx+1].file_offset -
            channel->segments[end_segment].time_series_indices_fps->time_series_indices[0].file_offset;
            num_blocks += end_idx + 1;
        }
        else {
            // case where end_idx is last block in segment
            total_samps += channel->segments[end_segment].metadata_fps->metadata.time_series_section_2->number_of_samples -
            channel->segments[end_segment].time_series_indices_fps->time_series_indices[0].start_sample;
            total_data_bytes += channel->segments[end_segment].time_series_data_fps->file_length -
            channel->segments[end_segment].time_series_indices_fps->time_series_indices[0].file_offset;
            num_blocks += end_idx + 1;
        }
        
        if (channel->segments[end_segment].time_series_indices_fps->time_series_indices[end_idx].file_offset < 1024){
            printf("Invalid index file offset, exiting...");
            if (read_channel == 1)
            {
                if (channel->number_of_segments > 0)
                    channel->segments[0].metadata_fps->directives.free_password_data = MEF_TRUE;
                free_channel(channel, MEF_TRUE);
            }
            return 0;
        }
    }
    
    // allocate buffers
    compressed_data_buffer = (ui1 *) malloc((size_t) total_data_bytes);
    cdp = compressed_data_buffer;
    
    // fill buffer with NAN's if specifiying by time.  No need to do this if specifying by sample.
    if (times_specified)
        memset_int(decomp_data, RED_NAN, num_samps);

    // read in RED data
    // normal case - everything is in one segment
    if (start_segment == end_segment) {
        if (channel->segments[start_segment].time_series_data_fps->fp == NULL)
            channel->segments[start_segment].time_series_data_fps->fp = fopen(channel->segments[start_segment].time_series_data_fps->full_file_name, "rb");
        fp = channel->segments[start_segment].time_series_data_fps->fp;
#ifndef _WIN32
        fseek(fp, channel->segments[start_segment].time_series_indices_fps->time_series_indices[start_idx].file_offset, SEEK_SET);
#else
        _fseeki64(fp, channel->segments[start_segment].time_series_indices_fps->time_series_indices[start_idx].file_offset, SEEK_SET);
#endif
        n_read = fread(cdp, sizeof(si1), (size_t) total_data_bytes, fp);
        if (read_channel == 1)
            fclose(fp);
        if (n_read != total_data_bytes){
            printf("Error reading file, exiting...");
            if (read_channel == 1)
            {
                if (channel->number_of_segments > 0)
                    channel->segments[0].metadata_fps->directives.free_password_data = MEF_TRUE;
                free_channel(channel, MEF_TRUE);
            }
            free (compressed_data_buffer);
            free (decomp_data);
            return 0;
        }
    }
    // spans across segments
    else {
        // start with first segment
        if (channel->segments[start_segment].time_series_data_fps->fp == NULL)
            channel->segments[start_segment].time_series_data_fps->fp = fopen(channel->segments[start_segment].time_series_data_fps->full_file_name, "rb");
        fp = channel->segments[start_segment].time_series_data_fps->fp;
#ifndef _WIN32
        fseek(fp, channel->segments[start_segment].time_series_indices_fps->time_series_indices[start_idx].file_offset, SEEK_SET);
#else
        _fseeki64(fp, channel->segments[start_segment].time_series_indices_fps->time_series_indices[start_idx].file_offset, SEEK_SET);
#endif
        bytes_to_read = channel->segments[start_segment].time_series_data_fps->file_length -
        channel->segments[start_segment].time_series_indices_fps->time_series_indices[start_idx].file_offset;
        n_read = fread(cdp, sizeof(si1), (size_t) bytes_to_read, fp);
        if (read_channel == 1)
            fclose(fp);
        if (n_read != bytes_to_read){
            printf("Error reading file, exiting...");
            if (read_channel == 1)
            {
                if (channel->number_of_segments > 0)
                    channel->segments[0].metadata_fps->directives.free_password_data = MEF_TRUE;
                free_channel(channel, MEF_TRUE);
            }
            free (compressed_data_buffer);
            free (decomp_data);
            return 0;
        }
        cdp += bytes_to_read;
        
        // this loop will only run if there are segments in between the start and stop segments
        for (i = (start_segment + 1); i <= (end_segment - 1); i++) {
            if (channel->segments[i].time_series_data_fps->fp == NULL)
                channel->segments[i].time_series_data_fps->fp = fopen(channel->segments[i].time_series_data_fps->full_file_name, "rb");
            fp = channel->segments[i].time_series_data_fps->fp;
#ifndef _WIN32
            fseek(fp, UNIVERSAL_HEADER_BYTES, SEEK_SET);
#else
            _fseeki64(fp, UNIVERSAL_HEADER_BYTES, SEEK_SET);
#endif
            bytes_to_read = channel->segments[i].time_series_data_fps->file_length -
            channel->segments[i].time_series_indices_fps->time_series_indices[0].file_offset;
            n_read = fread(cdp, sizeof(si1), (size_t) bytes_to_read, fp);
            if (read_channel == 1)
                fclose(fp);
            if (n_read != bytes_to_read){
                printf("Error reading file, exiting...");
                if (read_channel == 1)
                {
                    if (channel->number_of_segments > 0)
                        channel->segments[0].metadata_fps->directives.free_password_data = MEF_TRUE;
                    free_channel(channel, MEF_TRUE);
                }
                free (compressed_data_buffer);
                free (decomp_data);
                return 0;
            }
            cdp += bytes_to_read;
        }
        
        // then last segment
        num_block_in_segment = channel->segments[end_segment].metadata_fps->metadata.time_series_section_2->number_of_blocks;
        if (end_idx < (channel->segments[end_segment].metadata_fps->metadata.time_series_section_2->number_of_blocks - 1)) {
            if (channel->segments[end_segment].time_series_data_fps->fp == NULL)
                channel->segments[end_segment].time_series_data_fps->fp = fopen(channel->segments[end_segment].time_series_data_fps->full_file_name, "rb");
            fp = channel->segments[end_segment].time_series_data_fps->fp;
#ifndef _WIN32
            fseek(fp, UNIVERSAL_HEADER_BYTES, SEEK_SET);
#else
            _fseeki64(fp, UNIVERSAL_HEADER_BYTES, SEEK_SET);
#endif
            bytes_to_read = channel->segments[end_segment].time_series_indices_fps->time_series_indices[end_idx+1].file_offset -
            channel->segments[end_segment].time_series_indices_fps->time_series_indices[0].file_offset;
            n_read = fread(cdp, sizeof(si1), (size_t) bytes_to_read, fp);
            if (read_channel == 1)
                fclose(fp);
            if (n_read != bytes_to_read){
                printf("Error reading file, exiting...");
                if (read_channel == 1)
                {
                    if (channel->number_of_segments > 0)
                        channel->segments[0].metadata_fps->directives.free_password_data = MEF_TRUE;
                    free_channel(channel, MEF_TRUE);
                }
                free (compressed_data_buffer);
                free (decomp_data);
                return 0;
            }
            cdp += bytes_to_read;
        }
        else {
            // case where end_idx is last block in segment
            if (channel->segments[end_segment].time_series_data_fps->fp == NULL)
                channel->segments[end_segment].time_series_data_fps->fp = fopen(channel->segments[end_segment].time_series_data_fps->full_file_name, "rb");
            fp = channel->segments[end_segment].time_series_data_fps->fp;
#ifndef _WIN32
            fseek(fp, UNIVERSAL_HEADER_BYTES, SEEK_SET);
#else
            _fseeki64(fp, UNIVERSAL_HEADER_BYTES, SEEK_SET);
#endif
            bytes_to_read = channel->segments[end_segment].time_series_data_fps->file_length -
            channel->segments[end_segment].time_series_indices_fps->time_series_indices[0].file_offset;
            n_read = fread(cdp, sizeof(si1), (size_t) bytes_to_read, fp);
            if (read_channel == 1)
                fclose(fp);
            if (n_read != bytes_to_read){
                printf("Error reading file, exiting...");
                if (read_channel == 1)
                {
                    if (channel->number_of_segments > 0)
                        channel->segments[0].metadata_fps->directives.free_password_data = MEF_TRUE;
                    free_channel(channel, MEF_TRUE);
                }
                free (compressed_data_buffer);
                free (decomp_data);
                return 0;
            }
            cdp += bytes_to_read;
        }
    }
    
    // set up RED processing struct
    cdp = compressed_data_buffer;
    max_samps = channel->metadata.time_series_section_2->maximum_block_samples;
    
    // create RED processing struct
    rps = (RED_PROCESSING_STRUCT *) calloc((size_t) 1, sizeof(RED_PROCESSING_STRUCT));
    rps->compression.mode = RED_DECOMPRESSION;
    //rps->directives.return_block_extrema = MEF_TRUE;
    rps->decompressed_ptr = rps->decompressed_data = decomp_data;
    rps->difference_buffer = (si1 *) e_calloc((size_t) RED_MAX_DIFFERENCE_BYTES(max_samps), sizeof(ui1), __FUNCTION__, __LINE__, USE_GLOBAL_BEHAVIOR);
    
    // offset_to_start_samp = start_samp - channel->segments[start_segment].time_series_indices_fps->time_series_indices[start_idx].start_sample;
    
    sample_counter = 0;
    
    cdp = compressed_data_buffer;
    
    // TBD use real max block length
    temp_data_buf = (int *) malloc(33000 * 4);
    rps->decompressed_ptr = rps->decompressed_data = temp_data_buf;
    rps->compressed_data = cdp;
    rps->block_header = (RED_BLOCK_HEADER *) rps->compressed_data;
    if (!check_block_crc((ui1*)(rps->block_header), max_samps, compressed_data_buffer, total_data_bytes))
    {
        printf("RED block %lu has 0 bytes, or CRC failed, data likely corrupt...", start_idx);
        if (read_channel == 1)
        {
            if (channel->number_of_segments > 0)
                channel->segments[0].metadata_fps->directives.free_password_data = MEF_TRUE;
            free_channel(channel, MEF_TRUE);
        }
        free (compressed_data_buffer);
        free (decomp_data);
        free (temp_data_buf);
        return 0;
    }
    RED_decode(rps);
    cdp += rps->block_header->block_bytes;
    
    
    if (times_specified)
        offset_into_output_buffer = (int)((((rps->block_header->start_time - start_time) / 1000000.0) * channel->metadata.time_series_section_2->sampling_frequency) + 0.5);
    else
        offset_into_output_buffer = channel->segments[start_segment].time_series_indices_fps->time_series_indices[start_idx].start_sample - start_samp;
    
    // copy requested samples from first block to output buffer
    // TBD this loop could be optimized
    for (i=0;i<rps->block_header->number_of_samples;i++)
    {
        if (offset_into_output_buffer < 0)
        {
            offset_into_output_buffer++;
            continue;
        }
        
        if (offset_into_output_buffer >= num_samps)
            break;
        
        *(decomp_data + offset_into_output_buffer) = temp_data_buf[i];
        
        offset_into_output_buffer++;
    }
    
    // decode bytes to samples
    sample_counter = offset_into_output_buffer;
    for (i=1;i<num_blocks-1;i++) {
        rps->compressed_data = cdp;
        rps->block_header = (RED_BLOCK_HEADER *) rps->compressed_data;
        // check that block fits fully within output array
        // this should be true, but it's possible a stray block exists out-of-order, or with a bad timestamp
        
        // we need to manually remove offset, since we are using the time value of the block bevore decoding the block
        // (normally the offset is removed during the decoding process)
        
        if ((rps->block_header->block_bytes == 0) || !check_block_crc((ui1*)(rps->block_header), max_samps, compressed_data_buffer, total_data_bytes)){
            printf("RED block %lu has 0 bytes, or CRC failed, data likely corrupt...", start_idx+i);
            if (read_channel == 1)
            {
                if (channel->number_of_segments > 0)
                    channel->segments[0].metadata_fps->directives.free_password_data = MEF_TRUE;
                free_channel(channel, MEF_TRUE);
            }
            free (compressed_data_buffer);
            free (decomp_data);
            free (temp_data_buf);
            return 0;
        }
        
        if (times_specified){
            
            block_start_time_offset = rps->block_header->start_time;
            remove_recording_time_offset( &block_start_time_offset );
            
            if (block_start_time_offset < start_time)
                continue;
            if (block_start_time_offset + ((rps->block_header->number_of_samples / channel->metadata.time_series_section_2->sampling_frequency) * 1e6) >= end_time)
                continue;
            rps->decompressed_ptr = rps->decompressed_data = decomp_data + (int)((((block_start_time_offset - start_time) / 1000000.0) * channel->metadata.time_series_section_2->sampling_frequency) + 0.5);
            
        }else{
            rps->decompressed_ptr = rps->decompressed_data = decomp_data + sample_counter;
        }
        
        RED_decode(rps);
        cdp += rps->block_header->block_bytes;
        sample_counter += rps->block_header->number_of_samples;
    }
    
    if (num_blocks > 1)
    {
        // decode last block to temp array
        rps->compressed_data = cdp;
        rps->block_header = (RED_BLOCK_HEADER *) rps->compressed_data;
        rps->decompressed_ptr = rps->decompressed_data = temp_data_buf;
        if (!check_block_crc((ui1*)(rps->block_header), max_samps, compressed_data_buffer, total_data_bytes))
        {
            printf("RED block %lu has 0 bytes, or CRC failed, data likely corrupt...", start_idx+i);
            if (read_channel == 1)
            {
                if (channel->number_of_segments > 0)
                    channel->segments[0].metadata_fps->directives.free_password_data = MEF_TRUE;
                free_channel(channel, MEF_TRUE);
            }
            free (compressed_data_buffer);
            free (decomp_data);
            free (temp_data_buf);
            return 0;
        }
        RED_decode(rps);
        
        offset_into_output_buffer = (int)((((rps->block_header->start_time - start_time) / 1000000.0) * channel->metadata.time_series_section_2->sampling_frequency) + 0.5);
        
        if (times_specified)
            offset_into_output_buffer = (int)((((rps->block_header->start_time - start_time) / 1000000.0) * channel->metadata.time_series_section_2->sampling_frequency) + 0.5);
        else
            offset_into_output_buffer = sample_counter;
        
        // copy requested samples from last block to output buffer
        // TBD this loop could be optimized
        for (i=0;i<rps->block_header->number_of_samples;i++)
        {
            if (offset_into_output_buffer < 0)
            {
                offset_into_output_buffer++;
                continue;
            }
            
            if (offset_into_output_buffer >= num_samps)
                break;
            
            *(decomp_data + offset_into_output_buffer) = temp_data_buf[i];
            
            offset_into_output_buffer++;
        }
    }
    
    // copy requested samples from last block to output buffer
    // we're done with the compressed data, get rid of it
    free (temp_data_buf);
    free (compressed_data_buffer);
    free (rps->difference_buffer);
    free (rps);
    
    if (read_channel == 1)
    {
        if (channel->number_of_segments > 0)
            channel->segments[0].metadata_fps->directives.free_password_data = MEF_TRUE;
        free_channel(channel, MEF_TRUE);
    }
    
    
    return num_samps;
}

/**************************  Other helper functions  ****************************/

si8 sample_for_uutc_c(si8 uutc, CHANNEL *channel)
{
    ui8 i, j, sample;
    sf8 native_samp_freq;
    ui8 prev_sample_number;
    si8 prev_time, seg_start_sample;
    
    native_samp_freq = channel->metadata.time_series_section_2->sampling_frequency;
    prev_sample_number = channel->segments[0].metadata_fps->metadata.time_series_section_2->start_sample;
    prev_time = channel->segments[0].time_series_indices_fps->time_series_indices[0].start_time;
    
    for (j = 0; j < channel->number_of_segments; j++)
    {
        seg_start_sample = channel->segments[j].metadata_fps->metadata.time_series_section_2->start_sample;
        for (i = 0; i < channel->segments[j].metadata_fps->metadata.time_series_section_2->number_of_blocks; ++i) {
            if (channel->segments[j].time_series_indices_fps->time_series_indices[i].start_time > uutc)
                goto done;
            prev_sample_number = channel->segments[j].time_series_indices_fps->time_series_indices[i].start_sample + seg_start_sample;
            prev_time = channel->segments[j].time_series_indices_fps->time_series_indices[i].start_time;
        }
    }
    
done:
    sample = prev_sample_number + (ui8) (((((sf8) (uutc - prev_time)) / 1000000.0) * native_samp_freq) + 0.5);
    
    return(sample);
}

si8 uutc_for_sample_c(si8 sample, CHANNEL *channel)
{
    ui8 i, j, uutc;
    sf8 native_samp_freq;
    ui8 prev_sample_number;
    si8 prev_time, seg_start_sample;
    
    
    native_samp_freq = channel->metadata.time_series_section_2->sampling_frequency;
    prev_sample_number = channel->segments[0].metadata_fps->metadata.time_series_section_2->start_sample;
    prev_time = channel->segments[0].time_series_indices_fps->time_series_indices[0].start_time;
    
    for (j = 0; j < channel->number_of_segments; j++)
    {
        seg_start_sample = channel->segments[j].metadata_fps->metadata.time_series_section_2->start_sample;
        for (i = 0; i < channel->segments[j].metadata_fps->metadata.time_series_section_2->number_of_blocks; ++i){
            if (channel->segments[j].time_series_indices_fps->time_series_indices[i].start_sample + seg_start_sample > sample)
                goto done;
            prev_sample_number = channel->segments[j].time_series_indices_fps->time_series_indices[i].start_sample + seg_start_sample;
            prev_time = channel->segments[j].time_series_indices_fps->time_series_indices[i].start_time;
        }
    }
    
done:
    uutc = prev_time + (ui8) ((((sf8) (sample - prev_sample_number) / native_samp_freq) * 1000000.0) + 0.5);
    
    return(uutc);
}

void memset_int(si4 *ptr, si4 value, size_t num)
{
    si4 *temp_ptr;
    int i;
    
    if (num < 1)
        return;
    
    temp_ptr = ptr;
    for (i=0;i<num;i++)
    {
        memcpy(temp_ptr, &value, sizeof(si4));
        temp_ptr++;
    }
}

si4 check_block_crc(ui1* block_hdr_ptr, ui4 max_samps, ui1* total_data_ptr, ui8 total_data_bytes)
{
    ui8 offset_into_data, remaining_buf_size;
    si1 CRC_valid;
    RED_BLOCK_HEADER* block_header;
    
    offset_into_data = block_hdr_ptr - total_data_ptr;
    remaining_buf_size = total_data_bytes - offset_into_data;
    
    // check if remaining buffer at least contains the RED block header
    if (remaining_buf_size < RED_BLOCK_HEADER_BYTES)
        return 0;
    
    block_header = (RED_BLOCK_HEADER*) block_hdr_ptr;
    
    // check if entire block, based on size specified in header, can possibly fit in the remaining buffer
    if (block_header->block_bytes > remaining_buf_size)
        return 0;
    
    // check if size specified in header is absurdly large
    if (block_header->block_bytes > RED_MAX_COMPRESSED_BYTES(max_samps, 1))
        return 0;
    
    // at this point we know we have enough data to actually run the CRC calculation, so do it
    CRC_valid = CRC_validate((ui1*) block_header + CRC_BYTES, block_header->block_bytes - CRC_BYTES, block_header->block_CRC);
    
    // return output of CRC heck
    if (CRC_valid == MEF_TRUE)
        return 1;
    else
        return 0;
}

