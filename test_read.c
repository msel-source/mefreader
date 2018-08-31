// Multiscale Electrophysiology Format (MEF) version 3.0
// Copyright 2018, Mayo Foundation, Rochester MN. All rights reserved.
// Written by Matt Stead, Ben Brinkmann, and Dan Crepeau.

// Usage and modification of this source code is governed by the Apache 2.0 license.
// You may not use this file except in compliance with this License.
// A copy of the Apache 2.0 License may be obtained at http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under this License is distributed on an "as is" basis,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either expressed or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Thanks to all who acknowledge the Mayo Systems Electrophysiology Laboratory, Rochester, MN
// in academic publications of their work facilitated by this software.

// This code is a simple example in C of how to read data from a MEF 3.0 channel.

#include <stdlib.h>
#include <stdio.h>
#include "read_mef_ts_data.h"

int main()
{
    si1 channel_path[MEF_FULL_FILE_NAME_BYTES];
    si8 start_time, end_time;
    si8 start_samp, end_samp;
    sf8 sampling_frequency;
    si4 num_samps, samps_returned;
    si4 *samp_buf;
    
    // define channel and parameters
    MEF_strncpy(channel_path, "/Users/localadmin/Desktop/mef-example/ucd1_npc700183h_20180808103603.mefd/e1-e2.timd/", MEF_FULL_FILE_NAME_BYTES);
    start_time = 1533749749914432;        // GMT: 8 August 2018 17:35:49  (beginning of file)
    end_time = start_time + (10 * 1e6);   // 10 seconds later  (times are specified in microseconds)
    sampling_frequency = 250.0;
    
    // determine size of output buffer
    num_samps = (((double)(end_time - start_time) / 1e6) * sampling_frequency) + 1;  // add one to round up
    
    // allocate output buffer
    samp_buf = (si4*)calloc(num_samps, sizeof(si4));
    
    printf("***** Test 1, extracting samples by time range. *****\n");
    
    // get data
    samps_returned = read_mef_ts_data_by_time(channel_path, NULL, start_time, end_time, samp_buf, NULL);
    
    // output data
    printf("Samps returned: %d\n", samps_returned);
    for (int i=0;i<samps_returned;i++)
        printf("samp: %d, time: %ld\n", samp_buf[i], start_time + (si8)(i * (1e6/sampling_frequency)));
    
    printf("***** Test 2, extracting samples by sample range. *****\n");
    
    // set parameters for next test
    start_samp = 0;
    end_samp = start_samp + (10 * sampling_frequency);  // 10 seconds of data, at 250 sampling frequency
    
    // get data
    samps_returned = read_mef_ts_data_by_samp(channel_path, NULL, start_samp, end_samp, samp_buf, NULL);
    
    // output data
    printf("Samps returned: %d\n", samps_returned);
    for (int i=0;i<samps_returned;i++)
        printf("samp: %d\n", samp_buf[i]);
    
    printf("All done.\n");

    // free buffer
    free(samp_buf);
    
    return 1;
}
