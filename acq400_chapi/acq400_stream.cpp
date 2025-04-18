/*
 * acq400_stream.cpp
 *
 *  Created on: 28 July 2021
 *
 *  acq400_stream UUT [port] | pv > mybigrawfile
 *  streaming function stands in for
 *  nc UUT 4210 | pv > mybigrawfile
 *      Author: pgm
 */


#include <stdio.h>
#include <stdlib.h>
#include "acq400_chapi.h"

#include <string>
#include <iostream>
#include <bits/stdc++.h>

#define BUFLEN 0x10000

namespace G {
	unsigned long long maxbytes = 0;
};

template <class C>
int streamer(acq400_chapi::Acq400& uut, acq400_chapi::Ports port)
{
	C* buf = new C[BUFLEN];
	int nbuf;
	unsigned long long nbytes = 0;

	while ((nbuf = uut.stream(buf, BUFLEN, port)) > 0){
		fwrite(buf, sizeof(C), nbuf, stdout);
		nbytes += nbuf*sizeof(C);
		if (G::maxbytes && nbytes >= G::maxbytes){
			break;
		}
	}
	return 0;
}

int main(int argc, char **argv) {
	if (argc < 2){
		fprintf(stderr, "USAGE: acq400_stream UUT [PORT]\n");
		exit(1);
	}
	if (getenv("ACQ400_STREAM_MAXB")){
		G::maxbytes = strtoull(getenv("ACQ400_STREAM_MAXB"), 0, 0);
	}
	if (getenv("ACQ400_STREAM_MAXM")){
		G::maxbytes = strtoull(getenv("ACQ400_STREAM_MAXM"), 0, 0) * 0x100000;
	}
	const char* host = argv[1];
	acq400_chapi::Ports port = acq400_chapi::STREAM;
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

