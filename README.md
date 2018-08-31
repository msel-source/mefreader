# mefreader
Tools to read data from MEF 3.0 files

This module of code (read_mef_ts_data) is designed to be used in conjunction with the base
MEF 3.0 [library API](https://github.com/msel-source/meflib) (meflib and mefrec files).

Read_mef_ts_data is a module of code that allows extraction from a MEF 3.0 channel two different ways, by time value or by sample value.

In either case it is the user's job to allocate a decompression buffer of the appropriate size prior to calling the function.

The test code ("test_read.c") shows an example of use for both ways of calling the module.  In both cases the first 10 seconds of data are requested.  With the [sample data](https://github.com/msel-source/sampledata) provided, this will actually cause slightly different results, because there is a small gap, or discontinuity, within the first 10 seconds of the data.  The time-specificication approach will contain 39 samples of NaN (not-a-number, specified in MEF as -2^31) values to represent this gap.  The sample-specification approach will contain apparently continuous data, however, the returned 10 seconds of data will actually be slightly more than 10 seconds (time-wise) of the channel data, due to that small gap.  It is up to the user to determine which approach is preferable.

There are also two ways in which the functions can be called, in terms of specifying the channel name.  The first two parameters are the channel name and password.  If those are specified, then the last parameter should be left as NULL.  However, if the user has previously read the CHANNEL into a structure, then that CHANNEL can be passed as the last parameter, in which case the first two parameters (channel name and password) won't be used.  This option exists for efficiency.  If mutiple calls will be made to the same channel in rapid sucession, and the channel itself isn't changing, then it is more efficient to read the CHANNEL information once, rather that with each data extraction.

This software is licensed under the Apache software license 2.0. See [LICENSE](./LICENSE) for details.
