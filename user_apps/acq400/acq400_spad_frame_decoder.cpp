/*
 * acq400_spad_frame_decoder.cpp
 *
 *  Created on: 5 Feb 2024
 *      Author: pgm

SPAD_FRAME is a feature that adds a low overhead instrumentation frame to a sample data set.

acq400_spad_frame_decoder:
1. accepts a stream of SPAD_FRAMEs as input.
2. validates the frames, STOPS on ERROR.
3. counts state changes in the DI4 field and prints it out at the end of the run.
4. outputs an expanded data set that may be easier to log (so that clients do NOT need to know the full detail of the SPAD_FRAME

For reference, here is a detailed description of SPAD_FRAME.
The SPAD_FRAME is single u32 column at the end of every sample to include additional data:

 * If SPAD_FRAME is set to 1 then a single LWord is appended to every sample in the data stream and the data is recovered over multiple data Samples as follows | Sample Count | SEW2 | SEW1 | DI4 | FrameID

Frame	D31:24	D23:16	D15:18	D7:4	D3 (Subframe)
0		SCaa	SEW2aa	SEW1aa	DI4		1
1		SCbb	SEW2bb	SEW1bb	DI4		2
2		SCcc	SEW2cc	SEW1cc	DI4		3
3		SCdd	SEW2dd	SEW1dd	DI4		4
The DI4 data is presented in every frame thus allowing full rate DI data to be sent, only the Sample Count and the two SEW LWords need to be assembled as follows
The LWord is split into 4 bytes aabbccdd and sent most significant byte first

SEW1 SEW2 : "Software Embedded Word 1 and 2" :: 2 32 bit quantities that can be injected by embedded software. The values are embedded in the
data in sync with the sample. Example uses include: embedding time of day and/or position (perhaps from a GPS), or heading/pitch/roll from an IMU.

Example usage:

System: ACQ1001Q+ACQ465-32. This system is just able to stream 33 x u32 at full rate 374 kHz on TCP/IP

To configure the spad frame:
```
cat /mnt/local/sysconfig/transient.init
...
set.site 0 spad=2,0,0         # SPAD-FRAME
run0 1

acq1001_301> get.site 0 NCHAN # system has 33 logical channels.
33

In our system, we inject a logic waveform 10Hz into the DI on the TERM10/HDMI.


Data Format:

Showing ONE Full Sample:
Each channel has a unique id, 0x???????20, 0x??????21 .. 0x??????3f
```
pgm@hoy6:~/PROJECTS/acq400_chapi/user_apps/acq400$ hexdump -e '8/4 "%08x," "\n"' <bigrawfile | head -n 5
ffffc120,00000d21,fffef122,00000b23,ffffd824,0000af25,00005226,ffffde27,    CH01 .. CH08
ffff7528,ffff9929,0000c62a,ffffbd2b,ffff7e2c,ffffb62d,fffed32e,ffffb62f,    CH09 .. CH16
ffffaf30,ffff9031,ffff6032,fffffa33,ffff8c34,00012535,ffffee36,00007537,    CH17 .. CH24
00004138,ffff8539,fffeeb3a,0000433b,00004b3c,ffff323d,fffff83e,fffff63f,    CH25 .. CH32
00112202,                                                                   SPAD_FRAME
```

Dump two full SPAD_FRAMEs, we cut some data channels to make it fit the page:
```
pgm@hoy6:~/PROJECTS/acq400_chapi/user_apps/acq400$ hexdump -e '33/4 "%08x," "\n"' <bigrawfile | grep -n . | tr : , | cut -d, -f1,2-4,29- | head
1,ffffc120,00000d21,fffef122,0000433b,00004b3c,ffff323d,fffff83e,fffff63f,00112202,    # first sample in set ALWAYS starts at subframe 2
2,ffffb320,00000521,fffef722,0000413b,0000493c,ffff2d3d,fffff53e,ffffe93f,00112203,
3,ffffc320,00000a21,fffee922,0000373b,0000533c,ffff2c3d,ffffff3e,ffffed3f,03112204,    # first sample count is 3: correct!
4,ffffbb20,00000c21,fffee522,0000473b,00004f3c,ffff353d,0000023e,ffffe63f,00112201,    # Subframe 1
5,ffffb420,00000921,fffeec22,0000403b,0000493c,ffff3c3d,0000073e,fffff13f,00112202,	   # Subframe 2
6,ffffbd20,00000621,fffef222,0000323b,0000533c,ffff423d,fffff93e,fffff23f,00112203,    # Subframe 3
7,ffffb720,00000721,fffeec22,0000423b,0000553c,ffff393d,fffffe3e,fffff03f,07112204,    # Subframe 4 seventh sample is 7: correct
8,ffffbc20,00001021,fffee822,0000413b,0000533c,ffff283d,0000023e,fffff23f,00112201,    # Subframe 1 .. repeat
```

Optional output to an inflated frame.
The inflated frame decodes the SPAD FRAME so that the user does not have to, and presents the data in a handy one-column per item format.
It also computes the actual sample number for EVERY sample.
SEW1 and SEW2 are a useful opportunity to inject some information into the frame.
Here it's a static number, but it could be dynamic eg time_t or GPS NMEA or similar.

set.site 0 spad1 0x12345678    # inject SEW1
set.site 0 spad2 0x2bad1dea    # inject SEW2

result
```
nc acq1001_301 4210 | pv | ./acq400_spad_frame_decoder  -o - | hexdump -e '37/4 "%08x," "\n"' | cut -d, -f1-4,32- | head
ffffee20,ffffe521,ffff2822,ffffe823,0000473f,00122b81,00000001,00345678,00ad1dea,00000008,                                                                                                                                                        ]
ffffea20,ffffe621,ffff2b22,ffffea23,0000433f,00561d83,00000002,00345678,00ad1dea,00000008,
ffffea20,ffffe721,ffff2622,ffffe623,0000433f,0378ea84,00000003,00345678,00ad1dea,00000008,
ffffee20,ffffe521,ffff2822,ffffe823,0000473f,00122b81,00000004,12345678,2bad1dea,00000008,
ffffea20,ffffe221,ffff3322,ffffeb23,0000483f,0034ad82,00000005,12345678,2bad1dea,00000008,
ffffec20,ffffe121,ffff2c22,ffffec23,00004e3f,00561d83,00000006,12345678,2bad1dea,00000008,
ffffea20,ffffe621,ffff2a22,ffffee23,0000493f,0778ea84,00000007,12345678,2bad1dea,00000008,
ffffe620,ffffe021,ffff2e22,fffff223,0000433f,00122b81,00000008,12345678,2bad1dea,00000008,
ffffe820,ffffde21,ffff2d22,ffffef23,0000483f,0034ad82,00000009,12345678,2bad1dea,00000008,
ffffe420,ffffe421,ffff3122,ffffea23,00004d3f,00561d83,0000000a,12345678,2bad1dea,00000008,

```

Run for a period. Here we run for 30s, dumping the inflated data in real time.
It's probably best to slow the capture right down for this test:

```
set.site 0 sync_role master 10k               # slowest recommended operation rate.

(timeout -s2 30  nc acq1001_301 4210) |       # run for 30 s
pv |                                          # give capture status a ~1Hz
./acq400_spad_frame_decoder  -o - |           # decode the data and inflate the frame
hexdump -e '37/4 "%08x," "\n"' |              # dump 37 columns of data
cut -d, -f1-4,32-                             # limit the number of columns to fit the screen.

ffffe920,fffff621,ffff1f22,ffffed23,00004b3f,00122b81,00000001,00345678,00ad1dea,00000008,                                                                                                                                                        ]
ffffe920,fffff221,ffff2422,ffffeb23,00003f3f,00561d83,00000002,00345678,00ad1dea,00000008,
ffffdc20,fffff721,ffff2922,ffffef23,0000443f,0378ea84,00000003,00345678,00ad1dea,00000008,
ffffe920,fffff621,ffff1f22,ffffed23,00004b3f,00122b81,00000004,12345678,2bad1dea,00000008,
ffffea20,ffffe921,ffff2022,fffff223,0000493f,0034ad82,00000005,12345678,2bad1dea,00000008,
ffffe920,fffff221,ffff2022,ffffe923,0000433f,00561d83,00000006,12345678,2bad1dea,00000008,
ffffee20,fffff721,ffff2c22,ffffe723,0000473f,0778ea84,00000007,12345678,2bad1dea,00000008,
fffff420,ffffef21,ffff2d22,ffffee23,0000533f,00122b81,00000008,12345678,2bad1dea,00000008,

...
ffffef20,ffffe821,ffff2122,ffffee23,0000303f,eb78ea84,0003efeb,12345678,2bad1dea,00000008,
ffffea20,ffffea21,ffff2622,ffffe723,00003b3f,00122b81,0003efec,12345678,2bad1dea,00000008,
ffffdd20,ffffdf21,ffff2322,ffffe623,00003c3f,0334ad82,0003efed,12345678,2bad1dea,00000008,
ffffe620,ffffdf21,ffff1b22,ffffdb23,0000373f,ef561d83,0003efee,12345678,2bad1dea,00000008,
ffffea20,ffffe521,ffff2422,ffffd123,00003d3f,ef78ea84,0003efef,12345678,2bad1dea,00000008,
ffffea20,ffffdd21,ffff2322,ffffdd23,0000403f,00122b81,0003eff0,12345678,2bad1dea,00000008,
ffffed20,ffffe321,ffff2822,ffffe423,0000343f,0334ad82,0003eff1,12345678,2bad1dea,00000008,
ffffe920,ffffe921,ffff2b22,ffffdf23,0000313f,ef561d83,0003eff2,12345678,2bad1dea,00000008,
ffffe920,ffffe621,ffff2622,ffffe123,0000353f,f378ea84,0003eff3,12345678,2bad1dea,00000008,
```
DI Monitor:
acq400_spad_frame_decoder counts DI edges and publishes the result at the end:
```
DI:3 transitions 516 in 258050 samples
```

Translation: We have a 10Hz waveform, 512 transitions, two transitions per cycle. So, ~258 transitions in 258000 samples,
the ADC sample rate is 10k, the waveform is 10Hz, we have 1000x more samples than DI transitions. It looks about right..

Run a 30s shot at full rate, with validation and DI count at the end.

** DO NOT TRY to hexdump the data at high rates!

```
peter@naboo:~/PROJECTS/acq400_chapi/user_apps/acq400$ (timeout -s2 30  nc acq1001_301 4210) | pv |
./acq400_spad_frame_decoder -o - > /tmp/thirty_second.inflated37
 207MiB 0:00:07 [45.4MiB/s]
 447MiB 0:00:12 [47.3MiB/s]
 730MiB 0:00:18 [47.5MiB/s]
got one:2:00:29 [47.0MiB/s]
1.26GiB 0:00:30 [45.1MiB/s]
DI:3 transitions 549 in 10273954 samples
```

Check on the DI rate: 275 cycles in 30s, ~ 10Hz  10.3e6 samples in 30s => ~350kHz (it's not exactly 30s, there's some 3s delay at startyup)

Now we can dump the inflated data:

```
peter@naboo:~/PROJECTS/acq400_chapi/user_apps/acq400$ cat /tmp/thirty_second.inflated37 |
	hexdump -e '37/4 "%08x," "\n"' | cut -d, -f1-4,32- | head
fffff820,ffffd621,fffef222,ffffc023,fffff13f,00122b01,00000001,00345678,00ad1dea,00000000,
ffffe120,ffffa021,fffeec22,ffffa323,00001c3f,00561d03,00000002,00345678,00ad1dea,00000000,
ffff8d20,ffffbf21,ffff0322,ffffcc23,0000093f,0378ea04,00000003,00345678,00ad1dea,00000000,
fffff820,ffffd621,fffef222,ffffc023,fffff13f,00122b01,00000004,12345678,2bad1dea,00000000,
ffff9320,ffffb621,fffee222,ffffb023,00006a3f,0034ad02,00000005,12345678,2bad1dea,00000000,
ffff8f20,ffffae21,fffeec22,ffffb923,00006c3f,00561d03,00000006,12345678,2bad1dea,00000000,
00000920,ffffa021,ffff1e22,ffff8923,fffffd3f,0778ea04,00000007,12345678,2bad1dea,00000000,
ffffd720,ffffe821,ffff1422,ffffa223,0000023f,00122b01,00000008,12345678,2bad1dea,00000000,
ffffa720,fffffc21,fffef022,ffffd423,00002d3f,0034ad02,00000009,12345678,2bad1dea,00000000,
ffffb320,ffffaa21,ffff0f22,ffff9e23,ffffd73f,00561d03,0000000a,12345678,2bad1dea,00000000,
...
peter@naboo:~/PROJECTS/acq400_chapi/user_apps/acq400$ cat /tmp/thirty_second.inflated37 |
	hexdump -e '37/4 "%08x," "\n"' | cut -d, -f1-4,32- | tail
ffffd120,ffffca21,ffff0f22,ffffd523,0000243f,9b78ea84,009cc49b,12345678,2bad1dea,00000008,
ffffb520,ffff8721,fffef422,ffffd923,0000413f,00122b81,009cc49c,12345678,2bad1dea,00000008,
ffffb020,ffff8421,fffeba22,ffff5623,fffffd3f,9c34ad82,009cc49d,12345678,2bad1dea,00000008,
ffffa720,ffffae21,ffff0f22,ffffd123,ffffeb3f,c4561d83,009cc49e,12345678,2bad1dea,00000008,
ffff9520,ffffa521,fffeb822,ffffa723,0000533f,9f78ea84,009cc49f,12345678,2bad1dea,00000008,
ffff8520,ffffea21,fffebe22,ffff5623,ffffdc3f,00122b81,009cc4a0,12345678,2bad1dea,00000008,
ffffc020,ffff7921,fffef322,ffffd423,fffff93f,9c34ad82,009cc4a1,12345678,2bad1dea,00000008,
ffffdb20,ffff9221,fffea722,ffffaa23,00007b3f,c4561d83,009cc4a2,12345678,2bad1dea,00000008,
ffffd020,ffffda21,fffec822,ffff9023,fffffd3f,a378ea84,009cc4a3,12345678,2bad1dea,00000008,
```

Run a 3600s shot:
```
peter@naboo:~/PROJECTS/acq400_chapi/user_apps/acq400$ (timeout -s2 3600  nc acq1001_301 4210) | pv | ./acq400_spad_frame_decoder
30.6GiB 0:11:07 [46.5MiB/s]
30.6GiB 0:11:08 [47.4MiB/s]
45.6GiB 0:16:34 [47.2MiB/s]
69.0GiB 0:25:02 [47.6MiB/s]
77.0GiB 0:27:58 [50.0MiB/s]
87.8GiB 0:31:52 [47.4MiB/s]
 162GiB 0:58:48 [52.1MiB/s]
 165GiB 1:00:00 [44.5MiB/s]
got one:2

DI:3 transitions 71954 in 1345411066 samples

Manual:
35977 DI cycles in 3600s => 10Hz
1.345e9 samples in 3600s => 376 kHz
```

Testing multiple inputs:
10Hz -> CH01 on TERM05 -> DI3
10kHz -> CH04 on TERM05 -> DI0

result
```
peter@naboo:~/PROJECTS/acq400_chapi/user_apps/acq400$ timeout -s2 60 nc acq1001_301 4210 | pv  | ./acq400_spad_frame_decoder --nchan 16 | hexdump -e '21/4 "%08x," "\n"' | cut -d, -f16-
got one:2:00:59 [24.5MiB/s] [                                                                                                                                       <=>                      ]

1.37GiB 0:01:00 [24.0MiB/s] [                                                                                                                                   <=>                          ]
DI:0 transitions 1154873 in 21593850 samples
DI:3 transitions 1155 in 21593850 samples
```

=> It's easy to see that DI:0 is 1000x faster than DI:3 was
Actual rates:
1154873/2/21593850 * 374kHz = 10.00kHz
   1155/2/21593850 * 374kHz = 10Hz
 */



#include <stdio.h>
#include <unistd.h>
#include "acq400_chapi.h"

#include <string>
#include <iostream>
#include <bits/stdc++.h>

#include <popt.h>
#include <sys/uio.h>
#define BUFLEN 0x10000

#include <errno.h>
#include <signal.h>
#include <time.h>


#define STDOUT 1
#define STDERR 2

namespace G {
	const char* uut;
	int data32;
	int debug;
	char* outfile;
	int fd_out;
	int nchan = 32;
}

struct poptOption opt_table[] = {
	{ "nchan",      'n', POPT_ARG_INT, &G::nchan, 0   },
	{ "output",     'o', POPT_ARG_STRING, &G::outfile, 'o' },
	{ "debug",      'd', POPT_ARG_INT, &G::debug, 0   },
	POPT_AUTOHELP
	POPT_TABLEEND
};



void ui(int argc, const char **argv) {
	poptContext opt_context =
			poptGetContext(argv[0], argc, argv, opt_table, 0);
	int rc;
	while ( (rc = poptGetNextOpt( opt_context )) >= 0 ){
		switch(rc){
		case 'o':
			printf("hello %s\n", G::outfile);
			if (strcmp(G::outfile, "-") == 0){
				G::fd_out = STDOUT;
			}else{
				FILE *fp = fopen(G::outfile, "w");
				if (fp == 0){
					perror(G::outfile);
					exit(1);
				}else{
					G::fd_out = fileno(fp);
				}
			}
			break;
		default:
			break;
		}
	}
	/*
	G::uut = poptGetArg(opt_context);
	if (!G::uut){
		fprintf(stderr, "ERROR: must provide uut\n");
		exit(1);
	}
	*/
}

void init(acq400_chapi::Acq400& uut) {
	if (uut.get("0", "data32", G::data32) < 0){
		fprintf(stderr, "ERROR:");
		exit(1);
	}
}

typedef unsigned int u32;

const int QUAD = 4;

const int NDI = 4;
const u32 DI_PREVIOUS_NONE = 0xffffffffU;

template <int NCOL> class Sample {
		const int nchan;

		struct Metadata {
			u32 sc;
			u32 sew1;
			u32 sew2;
			u32 di;
		} meta[QUAD];

		u32 di_previous;

		unsigned long long DISTATES[2][NDI];
		u32 samples[QUAD][NCOL];
		struct iovec ov[QUAD*2];
		int i0;

		const int ssb;
		int vscalls;
		unsigned long long tot_samples;
public:
		Sample(const int _nchan): nchan(_nchan),
		    di_previous(DI_PREVIOUS_NONE),
			i0(0),
			ssb((_nchan+1)*sizeof(u32)), vscalls(0), tot_samples(0) {

			memset(meta, 0, sizeof(meta));
			memset(DISTATES, 0, sizeof(DISTATES));

			for (int ii = 0; ii < QUAD; ++ii){
				ov[ii*2].iov_base = samples[ii];
				ov[ii*2].iov_len  = ssb;
				ov[ii*2+1].iov_base = &meta[ii];
				ov[ii*2+1].iov_len  = sizeof(struct Metadata);
			}
		}
		void count_states(unsigned di4){
			if (di_previous != DI_PREVIOUS_NONE){
				u32 changed = di_previous^di4;
				if (changed){
					for (int ii = 0; ii < NDI; ++ii){
						if (changed&(1<<ii)){
							bool is_one = di4 | (1<<ii);
							DISTATES[is_one][ii] += 1;
						}
					}

				}
			}
			di_previous = di4;
		}
		void print_states(){
			for (int ii = 0; ii < NDI; ++ii){
				unsigned long long transitions = (DISTATES[1][ii]+DISTATES[0][ii]);
				if (transitions){
					fprintf(stderr, "DI:%d transitions %llu in %llu samples\n", ii, transitions, tot_samples);
				}
			}
		}

		/* apply to data channels 1..N */
		unsigned ch_id(u32 smpl1){
					return smpl1&0x000000ff;
		}
		/* apply to SPAD_FRAME word only */
		int frame(u32 *smpl){
			return smpl[nchan]&0x0000000f;
		}
		unsigned char scbyte(u32* smpl){
			return smpl[nchan] >> 24;
		}
		unsigned char di4(u32* smpl){
			return (smpl[nchan] >> 4) & 0x0f;
		}
		unsigned sew1(u32* smpl1){
			return (smpl1[nchan]&0x00ff0000) >> 16;
		}
		unsigned sew2(u32* smpl1){
			return (smpl1[nchan]&0x0000ff00) >> 8;
		}

		unsigned ch_id(int ii){
			return 0x20+ii;
		}
		bool valid_sample(u32* smpl){
			return ch_id(smpl[ 0]) == ch_id(0) &&
				   ch_id(smpl[ 1]) == ch_id(1) &&
				   ch_id(smpl[nchan-1]) == ch_id(nchan-1) &&
				   frame(smpl) >= 1 && frame(smpl) <= 4;
		}
		int decode(int nbytes){
			int last_frame = i0? frame(samples[i0-1]): 0;
			int ii;
			int frm;

			for (ii = i0; ii < QUAD; ++ii, last_frame = frm){
				++vscalls;
				if (!valid_sample(samples[ii])){
					int ignore = write(1, samples[ii], ssb);
					fprintf(stderr, "ERROR: not a valid sample at %d\n", vscalls);
					exit(1);
				}else{
					frm = frame(samples[ii]);
					if (tot_samples != 0 && frm != last_frame+1){
						fprintf(stderr, "ERROR: frm:%u last:%u frame sequence error\n", frm, last_frame);
						exit(1);
					}
					++tot_samples;
					if (frm == 4){
						i0 = (ii < QUAD-1)? ii+1: 0;  // tee up for next time.
						break;
					}
				}
			}
			if (frm != 4){
				fprintf(stderr, "ERROR: failed to quit at FRM=4 %d\n", frm);
				exit(1);
			}
			u32 sc = 0;

			unsigned _sew1 = 0, _sew2 = 0;

			for (int ic = ii, shl=0; ic>=0; --ic, shl+=8){
				sc |= scbyte(samples[ic]) << shl;
				_sew1 |= sew1(samples[ic]) << shl;
				_sew2 |= sew2(samples[ic]) << shl;
				//fprintf(stderr, "sc:%08x %d\n", sc, ic);
			}
			if (G::debug) fprintf(stderr, "sc:%08x\n", sc);


			for (int ic = 0; ic <= ii; ++ic){
				meta[ic].sc = sc - (ii - ic);
				meta[ic].sew1 = _sew1;
				meta[ic].sew2 = _sew2;
				meta[ic].di = di4(samples[ic]);
				/* additional debug */ /*
				meta[ic].di = frame(samples[ic])<<24 | ii<<16 | di4(samples[ic]);
				*/
				count_states(di4(samples[ic]));
			}

			/* we may have residue subframes, shuffle to the top for next time, setting i0 as the start point */
			int i2 = 0;
			for (int i1=ii+1; i1 < QUAD; ++i1, ++i2){
				memcpy(samples[i2], samples[i1], ssb);
			}
			i0 = i2;

			return 0;
		}
		int read(FILE* fp) {
			int rc = fread(&samples[i0], sizeof(u32)*NCOL, QUAD-i0, fp);
			if (G::debug) fprintf(stderr, "rc:%d\n", rc);
			if (rc > 0 && decode(rc) == 0){
				if (G::debug) fprintf(stderr, "call writev:%d\n", 2*(QUAD-i0));
				if (G::fd_out){
					return writev(1, ov, 2*(QUAD-i0));
				}else{
					return 1;
				}
			}else{
				return -1;
			}
		}
		void run(FILE* fp){
			while(read(fp) > 0){
				;
			}
			print_states();
		}
};

void catch_int(int sig_num)
{
	fprintf(stderr, "got one:%d\n\n", sig_num);
}

int main(int argc, const char **argv) {
	ui(argc, argv);
	signal(SIGINT, catch_int);
	signal(SIGPIPE, catch_int);

	switch (G::nchan){
	case 16:
		(new Sample<16+1>(G::nchan))->run(stdin); break;
	case 32:
		(new Sample<32+1>(G::nchan))->run(stdin); break;
	case 64:
		(new Sample<64+1>(G::nchan))->run(stdin); break;
	default:
		fprintf(stderr, "ERROR, sorry nchan=%d not supported\n", G::nchan);
	}
}
