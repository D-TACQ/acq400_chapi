/*
 * acq400_stream_awg.cpp
 *
 *  Created on: 3 September 2021
 *
 *  cat awg_data_file | acq400_stream_awg UUT [port]
 *  streaming function stands in for
 *  cat awg_data_file | nc UUT 54207
 *      Author: pgm
 */


#include <stdio.h>
#include "acq400_chapi.h"

#include <string>
#include <iostream>
#include <bits/stdc++.h>

#define BUFLEN 0x10000

template <class C>
int streamer(acq400_chapi::Acq400& uut, acq400_chapi::Ports port)
{
	C* buf = new C[BUFLEN];
	int nbuf;
	while ((nbuf = fread(buf, sizeof(C), BUFLEN, stdin)) > 0){
		uut.stream_out(buf, nbuf, port);
	}
	return 0;
}

int main(int argc, char **argv) {
	if (argc < 2){
		fprintf(stderr, "USAGE: acq400_stream_awg UUT [PORT]\n");
		exit(1);
	}
	const char* host = argv[1];
	acq400_chapi::Ports port = acq400_chapi::AWG_STREAM;
	if (argc > 2){
		acq400_chapi::Ports port = static_cast<acq400_chapi::Ports>(atoi(argv[2]));
	}

	acq400_chapi::Acq400 uut(host);
	int data32;
	if (uut.get("0", "data32", data32) < 0){
		fprintf(stderr, "ERROR:");
		exit(1);
	}
	if (data32){
		return streamer<long>(uut, port);
	}else{
		return streamer<short>(uut, port);
	}

}

