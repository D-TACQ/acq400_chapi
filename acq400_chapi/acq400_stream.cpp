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
#include "acq400_chapi.h"

#include <string>
#include <iostream>
#include <bits/stdc++.h>

#define BUFLEN 0x10000

int main(int argc, char **argv) {
	if (argc < 2){
		fprintf(stderr, "USAGE: acq400_stream UUT [PORT]\n");
		exit(1);
	}
	const char* host = argv[1];
	acq400_chapi::Ports port = acq400_chapi::STREAM;

	acq400_chapi::Acq400 uut(host);

	short* buf = new short[BUFLEN];
	int nbuf;
	while ((nbuf = uut.stream(buf, BUFLEN, port)) > 0){
		fwrite(buf, sizeof(short), nbuf, stdout);
	}
	return 0;
}

