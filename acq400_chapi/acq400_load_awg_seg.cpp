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

int G_noarm = 0;              // disable arming the awg. For stealth data update on running system

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
char load_seg(acq400_chapi::Acq400& uut, const char* sel)
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
	G_segments.insert(G_segments.begin(), seg);
	return 0;
}

const char* TRG_SRC_0 = "SIG:SRC:TRG:0";
char* G_trigger_stash;

void disable_fp_trigger(acq400_chapi::Acq400& uut)
{
	std::string response;
	std::string site0 = "0";
	uut.get(response, site0, TRG_SRC_0);
	std::vector<std::string> pv_value;

	split(response, ' ', pv_value);
	const char* value = pv_value[1].c_str();
	G_trigger_stash = new char[strlen(value)+1];
	strcpy(G_trigger_stash, value);

	uut.set(response, site0, "%s %s", TRG_SRC_0, "NONE");
	printf("disable_fp_trigger() \"%s\" was \"%s\" set to \"%s\"\n",
			TRG_SRC_0, G_trigger_stash, "NONE");
}

void restore_fp_trigger(acq400_chapi::Acq400& uut){
	std::string response;
	std::string site1 = "1";
	uut.set(response, site1, "trg=1,0,1");
	std::string site0 = "0";
	uut.set(response, site0, "%s %s", TRG_SRC_0, "EXT");
	printf("restore_fp_trigger() restore \"%s\" to \"%s\"\n",
			TRG_SRC_0, G_trigger_stash);
}

void set_playloop_len_disable(acq400_chapi::Acq400& uut, bool disable)
{
	std::string site0 = "0";
	uut.set(site0, "playloop_len_disable", (int)disable);
}
void select_awg_seg(int* pskt, acq400_chapi::Acq400& uut, char seg)
{
	bool first_time = *pskt == 0;

	uut.select_awg_seg(pskt, uut, seg);

	if (first_time && G_trigger_stash != 0){
		restore_fp_trigger(uut);
	}
}

void stop_awg(acq400_chapi::Acq400& uut)
{
	std::string response;
	std::string site1 = "1";

	uut.set(response, site1, "%s %s", "playloop_length", "0 0");
}

#include <csignal>
#include <iostream>


bool G_request_quit;

void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    // Cleanup and close up stuff here
    G_request_quit = true;
}

void iterate_segments(acq400_chapi::Acq400& uut)
{
	const int imax = G_segments.size();
	int skt = 0;
	int ii = 0;
	int iter = 0;
#define LIMIT(c) ((c)+1>=20)
#define INITFILL	4                    // Prime the queue to start

	signal(SIGINT, signalHandler);

	for (int col=0; !G_request_quit; col = LIMIT(col)? 0: col+1,
			 ii = (ii+1 >= imax? 0: ii+1)){

		char seg = G_segments[ii];

		select_awg_seg(&skt, uut, seg);
		printf("%c%c", seg, LIMIT(col)? '\n': ' '); fflush(stdout);
		if (++iter > INITFILL){
			usleep(G_switch_seg*1000);
		}
	}
	close(skt);
	skt = 0;
	select_awg_seg(&skt, uut, 'A');
	printf("iterate_segments() stop_awg()\n");
	stop_awg(uut);
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
	if ((opt = getenv("ACQ400_LAS_NOARM"))){
		G_noarm = atoi(opt);
	}

	acq400_chapi::Acq400 uut(host);

	if (G_noarm == 0){
		disable_fp_trigger(uut);
	}

	int data32;
	if (uut.get("0", "data32", data32) < 0){
		fprintf(stderr, "ERROR:");
		exit(1);
	}

	/* load segments in reverse order. only the last load ENABLES the AWG */
	set_playloop_len_disable(uut, true);
	for (int ii = argc-1; ii >= 2; --ii){
		if (ii == 2 && !G_noarm){
			set_playloop_len_disable(uut, false);
		}
		(data32? load_seg<long>: load_seg<short>)(uut, argv[ii]);
	}

	if (G_switch_seg == SWITCH_LAST){
		int skt = 0;
		select_awg_seg(&skt, uut, G_segments.back());
		close(skt);
	}else if (G_switch_seg == SWITCH_FIRST){
		int skt = 0;
		select_awg_seg(&skt, uut, G_segments.front());
		close(skt);
	}else if (G_switch_seg > 0){
		iterate_segments(uut);
	}
}

