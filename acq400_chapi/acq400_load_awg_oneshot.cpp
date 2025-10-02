/*
 * acq400_load_awg_oneshot.cpp
 *
 *  Created on: 01 October 2025
 *
 *  [OPTS] acq400_load_awg_oneshot UUT FILE [SEG=FILE]
 *  Where
 *  UUT                     # ip or DNS name of ACQ400 unit as normal
 *  SEG                     # [A-Z] load segment (subject to boot time limit)
 *  FILE                    # binary file to load. All files MUST be same size
 *  SEG without [=FILE]     # select segment, no load
 *  Where OPTS are:
 *  ACQ400_AUTO_SOFT_TRIGGER=0|1
 *
 *  also
 *  USAGE: [OPTS] acq400_load_awg_seg UUT SEG=FILE [SEG=FILE]
 *      Author: pgm
 */

#include <iostream>
#include <chrono>


#include "acq400_chapi.h"
#include "acq400_chapi_inc.h"






#define USAGE "USAGE: OPTS] acq400_load_awg_seg UUT SEG=FILE [SEG=FILE]\n"

enum Mode { ONESHOT, ARP, CON };     // AutoRepeatOnTrigger, CONtinuous


Mode G_mode = ONESHOT;

#include <sys/stat.h>
#include <string>

long get_file_size(const std::string& filename) {
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

acq400_chapi::Ports get_port(){
	switch(G_mode){

	case ARP:
		return acq400_chapi::Ports::AWG_AUTOREARM;
	case CON:
		return acq400_chapi::Ports::AWG_CONTINUOUS;
	case ONESHOT:
		return acq400_chapi::Ports::AWG_ONCE;
	default:
		fprintf(stderr, "ERROR bad mode %d\n", G_mode);
		exit(1);
	}
}
std::string dist_s1;

long G_file_size;

#define BUFLEN 0x10000

template <class C>
int awg_loader(acq400_chapi::Acq400& uut, acq400_chapi::Ports port, FILE* fp)
{
	C* buf = new C[BUFLEN];
	int nbuf = 0;
	int skt = 0;

	while ((nbuf = fread(buf, sizeof(C), BUFLEN, fp)) > 0){
		uut.stream_out(&skt, buf, nbuf, port);
	}

	uut.stream_out(&skt, buf, 0, port);
	return 0;
}


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
	}else if (G_file_size <= 4*bl){
		blp = G_file_size/4;
	}else{
		/* find blp such that (4+2nn) * blp == G_file_size */
		for (int nn = 1; nn < 512; ++nn){
			int awg_bufs = 4+2*nn;
			blp = G_file_size/awg_bufs;
			if (blp <= bl){
				break;
			}
		}
		if (!(blp <= bl)){
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
char load_awg(acq400_chapi::Acq400& uut, const char* fname)
{
	G_file_size = get_file_size(fname);
	match_buffer_len(uut, fname);
	FILE* fp = fopen(fname, "rb");
	if (fp == 0){
		fprintf(stderr, "ERROR failed to open file \"%s\"\n", fname);
		exit(1);
	}

	auto start = std::chrono::high_resolution_clock::now();
	::awg_loader<C>(uut, get_port(), fp);
	auto end = std::chrono::high_resolution_clock::now();

	auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

	double MB = (double)G_file_size/0x100000;
	double sec = (double)duration/1e9;
	std::cout << "TIMING:loaded port: " << get_port() << " : " << MB << "MB in "
		  << std::fixed << std::setprecision(2) << sec << " sec "
		  << MB/sec << " MB/s" << std::endl;
	fclose(fp);

	return 0;
}

const char* TRG_SRC_0 = "SIG:SRC:TRG:0";
char* G_trigger_stash;

int main(int argc, char **argv) {
	if (argc < 3){
		fprintf(stderr, USAGE);
		exit(1);
	}

	const char* host = argv[1];
	const char* data_file = argv[2];

	const char* opt;
	if ((opt = getenv("ACQ400_LAS_MODE")) != 0){
		if (strcmp(opt, "ARP") == 0){
			G_mode = ARP;
		}else if (strcmp(opt, "CON") == 0){
			G_mode = CON;
		}else{
			G_mode = ONESHOT;
		}
	}

	acq400_chapi::Acq400 uut(host);

	int data32;
	if (uut.get("0", "data32", data32) < 0){
		fprintf(stderr, "ERROR:");
		exit(1);
	}

	std::string response;

	if (uut.set(response, "1", "AWG:DIST=%s", "AWG")){
		fprintf(stderr, "ERROR: failed to select AWG:DIST=AWG");
		exit(1);
	}
	if (uut.get(::dist_s1, "0", "dist_s1" ) < 0){
		fprintf(stderr, "ERROR:");
		exit(1);
	}
	if (!IN_RANGE(std::stoi(::dist_s1), 1, 6)){
		fprintf(stderr, "ERROR: dist_s1 is not set 1-6 %d\n", std::stoi(::dist_s1));
		exit(1);
	}

	uut.set_playloop_len_disable(true);


	if (data32){
		load_awg<long>(uut, data_file);
	}else{
		load_awg<short>(uut, data_file);
	}
}

