/*
 * acq400_upload.cpp
 *
 *  Created on: 21 Oct 2021
 *      Author: pgm
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

/*
 * If SPAD_FRAME is set to 1 then a single LWord is appended to every sample in the data stream and the data is recovered over multiple data Samples as follows | Sample Count | SEW2 | SEW1 | DI4 | FrameID

Frame	D31:24	D23:16	D15:18	D7:4	D3:0
0	SCaa	SEW2aa	SEW1aa	DI4	1
1	SCbb	SEW2bb	SEW1bb	DI4	2
2	SCcc	SEW2cc	SEW1cc	DI4	3
3	SCdd	SEW2dd	SEW1dd	DI4	4
The DI4 data is presented in every frame thus allowing full rate DI data to be sent, only the Sample Count and the two SEW LWords need to be assembled as follows
The LWord is split into 4 bytes aabbccdd and sent most significant byte first

 */
namespace G {
	const char* uut;
	int data32;
	int debug;
}

struct poptOption opt_table[] = {
	{ "debug",      'd', POPT_ARG_INT, &G::debug, 0   },
	POPT_AUTOHELP
	POPT_TABLEEND
};


void ui(int argc, const char **argv) {
	poptContext opt_context =
			poptGetContext(argv[0], argc, argv, opt_table, 0);
	int rc;
	while ( (rc = poptGetNextOpt( opt_context )) >= 0 ){
		;
	}
	G::uut = poptGetArg(opt_context);
	if (!G::uut){
		fprintf(stderr, "ERROR: must provide uut\n");
		exit(1);
	}
}

void init(acq400_chapi::Acq400& uut) {
	if (uut.get("0", "data32", G::data32) < 0){
		fprintf(stderr, "ERROR:");
		exit(1);
	}
}

typedef unsigned int u32;

const int QUAD = 4;

class Sample {
		const int nchan;

		struct Metadata {
			u32 sc;
			u32 di;
		} meta[QUAD];
		u32* samples[QUAD];
		struct iovec iov[QUAD];
		struct iovec ov[QUAD*2];
		int i0;

		const int ssb;
		int vscalls;
		unsigned long long tot_samples;
public:
		Sample(const int _nchan): nchan(_nchan), i0(0),
			ssb((_nchan+1)*sizeof(u32)), vscalls(0), tot_samples(0) {

			memset(meta, 0, sizeof(meta));

			for (int ii = 0; ii < QUAD; ++ii){
				samples[ii] = new u32[nchan+1];
				iov[ii].iov_base = samples[ii];
				iov[ii].iov_len  = ssb;
				ov[ii*2].iov_base = samples[ii];
				ov[ii*2].iov_len  = ssb;
				ov[ii*2+1].iov_base = &meta[ii];
				ov[ii*2+1].iov_len  = sizeof(struct Metadata);
			}
		}
		int frame(u32 *smpl){
			return smpl[32]&0x0000000f;
		}
		unsigned char scbyte(u32* smpl){
			return smpl[32] >> 24;
		}
		unsigned char di4(u32* smpl){
			return (smpl[32] >> 4) & 0x0f;
		}
		unsigned ch_id(u32 smpl1){
			return smpl1&0x000000ff;
		}
		bool valid_sample(u32* smpl){
			return ch_id(smpl[ 0]) == 0x20 &&
				   ch_id(smpl[ 1]) == 0x21 &&
				   ch_id(smpl[31]) == 0x3f &&
				   frame(smpl) >= 1 && frame(smpl) <= 4;
		}
		int decode(int nbytes){
			int last_frame = i0? frame(samples[i0-1]): 0;
			int ii;
			int frm;

			for (ii = i0; ii < QUAD; ++ii, last_frame = frm){
				++vscalls;
				if (!valid_sample(samples[ii])){
					write(1, samples[ii], ssb);
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
			for (int ic = ii, shl=0; ic; --ic, shl+=8){
				sc |= scbyte(samples[ic]) << shl;
				//fprintf(stderr, "sc:%08x %d\n", sc, ic);
			}
			fprintf(stderr, "sc:%08x\n", sc);

			for (int ic = 0; ic <= ii; ++ic){
				meta[ic].sc = sc - (ii - ic);
				meta[ic].di = di4(samples[ic]);
			}

			int i2 = 0;
			for (int i1=ii+1; i1 < QUAD; ++i1, ++i2){
				memcpy(samples[i2], samples[i1], ssb);
			}
			i0 = i2;

			return 0;
		}
		int read(int fd) {
			int rc = readv(fd, iov+i0, QUAD-i0);
			fprintf(stderr, "rc:%d\n", rc);
			if (rc > 0 && decode(rc) == 0){
				fprintf(stderr, "call writev:%d\n", 2*(QUAD-i0));
				return writev(1, ov, 2*(QUAD-i0));
//				return 1;
			}else{
				return -1;
			}
		}
};


int main(int argc, const char **argv) {
	Sample sample(32);
	while(sample.read(0) > 0){
		;
	}
}
