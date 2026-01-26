/*
 * acq400_wait_for_ready.cpp
 *
 *  Created on: 28 January 2026
 *
 *  acq400_wait_for_ready UUT
 *  exit 0 when good
 *      Author: pgm
 */


#include "acq400_chapi.h"
#include "acq400_chapi_inc.h"



int main(int argc, char **argv) {
	if (argc < 2){
		fprintf(stderr, "USAGE: acq400_wait_for_ready UUT\n");
		exit(1);
	}

	const char* host = argv[1];
	acq400_chapi::Acq400 uut(host);
	return uut.wait_for_ready();
}

