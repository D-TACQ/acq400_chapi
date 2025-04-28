/*
 * acq400_load_awg_seg.cpp
 *
 *  Created on: 28 April 2025
 *
 *  [OPTS] acq400_load_awg_seg UUT SEG=FILE [SEG=FILE]
 *  Where
 *  UUT                     # ip or DNS name of ACQ400 unit as normal
 *  SEG                     # [A-Z] load segment (subject to boot time limit)
 *  FILE                    # binary file to load. All files MUST be same size
 *  SEG without [=FILE]     # select segment, no load
 *  Where OPTS are:
 *  ACQ400_LAS_MODE={ARP,CON}   # continuous, oneshot auto (default)
 *  ACQ400_LAS_SWITCH_SEG   # set segment on load
 *
 *  also
 *  USAGE: [OPTS] acq400_load_awg_seg UUT SEG=FILE [SEG=FILE]
 *      Author: pgm
 */


#include <stdio.h>
#include "acq400_chapi.h"

#include <string>
#include <iostream>
#include <vector>
#include <bits/stdc++.h>
#include "acq400_chapi_inc.h"
#define BUFLEN 0x10000


template <class C>
int loader(acq400_chapi::Acq400& uut, acq400_chapi::Ports port, FILE* fp)
{
	C* buf = new C[BUFLEN];
	int nbuf = 0;
	int skt = 0;

	while ((nbuf = fread(buf, sizeof(C), BUFLEN, fp)) > 0){
		uut.stream_out(&skt, buf, nbuf, port);
	}

	close(skt);
	return 0;
}

#define USAGE "USAGE: OPTS] acq400_load_awg_seg UUT SEG=FILE [SEG=FILE]\n"

enum Mode { ARP, CON };     // AutoRepeatOnTrigger, CONtinuous

enum SwitchSeg { NOSWITCH, SWITCH_LAST = -1, SWITCH_FIRST=1 /* switch msec = N > 1 */ };

Mode G_mode = ARP;
int G_switch_seg = NOSWITCH;

#include <sys/stat.h>
#include <string>

long get_file_size(const std::string& filename) {
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}



std::vector<char> G_segments;
long G_file_size;


void match_buffer_len(acq400_chapi::Acq400& uut, const char* fname)
{
	std::string site0 = "0";
	int bl;
	int blp;
	if (uut.get(site0, "bufferlen", bl) <= 0){
		fprintf(stderr, "ERROR: failed to read bufferlen\n");
		exit(1);
	}
	if (G_file_size < bl){
		blp = G_file_size;
	}else if (G_file_size < 4*bl){
		blp = G_file_size/4;
	}else{
		/* find blp such that (4+2n) * blp == G_file_size */
		for (int nn = 1; nn < 10; ++nn){
			blp = G_file_size/(4*2*nn);
			if (blp < bl){
				break;
			}
		}
		if (!(blp < bl)){
			fprintf(stderr, "ERROR: file will not fit\n");
			exit(1);
		}
	}
	if (uut.set(site0, "dist_bufferlen_play", blp)){
		fprintf(stderr, "ERROR: failed to set dist_bufferlen_play\n");
		exit(1);
	}
}

template<class C>
int load_seg(acq400_chapi::Acq400& uut, const char* sel)
{
	std::vector<std::string> seg_file;
	split(sel, '=', seg_file);
	const char seg = seg_file[0].c_str()[0];

	if (seg_file.size() > 1){
		const char* fname = seg_file[1].c_str();

		if (G_file_size == 0){
			G_file_size = get_file_size(fname);
			match_buffer_len(uut, fname);
		}else if (get_file_size(fname) != G_file_size){
			fprintf(stderr, "ERROR: file sizes do not match\n");
			exit(1);
		}

		FILE* fp = fopen(fname, "rb");
		if (fp == 0){
			fprintf(stderr, "ERROR failed to open file \"%s\"\n", fname);
			exit(1);
		}
		acq400_chapi::Ports port = (G_mode == ARP?
				acq400_chapi::AWG_SEG_ARP:
				acq400_chapi::AWG_SEG_CON  )(seg);
		loader<C>(uut, port, fp);
		fclose(fp);
	}
	G_segments.push_back(seg);
	return 0;
}


void iterate_segments(acq400_chapi::Acq400& uut)
{
	const int imax = G_segments.size();
	int skt = 0;
	int ii = 0;
#define LIMIT(c) ((c)+1>=20)

	for (int col=0;; col = LIMIT(col)? 0: col+1,
			 ii = (ii+1 >= imax? 0: ii+1)){

		char seg = G_segments[ii];

		uut.select_awg_seg(&skt, uut, seg);
		printf("%c%c", seg, LIMIT(col)? '\n': ' '); fflush(stdout);
		usleep(G_switch_seg*1000);
	}
	close(skt);
}

int main(int argc, char **argv) {
	if (argc < 3){
		fprintf(stderr, USAGE);
		exit(1);
	}

	const char* host = argv[1];

	const char* opt;
	if ((opt = getenv("ACQ400_LAS_MODE")) != 0){
		if (strcmp(opt, "ARP") == 0){
			G_mode = ARP;
		}else if (strcmp(opt, "CON") == 0){
			G_mode = CON;
		}else{
			fprintf(stderr, "ERROR: ACQ400_LAS_MODE supported: ARP, CON\n");
			return -1;
		}
	}
	if ((opt = getenv("ACQ400_LAS_SWITCH_SEG"))){
		G_switch_seg = atoi(opt);
	}

	acq400_chapi::Acq400 uut(host);
	int data32;
	if (uut.get("0", "data32", data32) < 0){
		fprintf(stderr, "ERROR:");
		exit(1);
	}

	for (int ii = 2; ii < argc; ++ii){
		(data32? load_seg<long>: load_seg<short>)(uut, argv[ii]);
	}

	if (G_switch_seg == SWITCH_LAST){
		int skt = 0;
		uut.select_awg_seg(&skt, uut, G_segments.back());
		close(skt);
	}else if (G_switch_seg == SWITCH_FIRST){
		int skt = 0;
		uut.select_awg_seg(&skt, uut, G_segments.front());
		close(skt);
	}else if (G_switch_seg > 0){
		iterate_segments(uut);
	}
}

