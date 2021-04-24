
#include <stdio.h>
#include "acq400_chapi.h"

#include <string>
#include <iostream>
#include <bits/stdc++.h>

int main(int argc, char **argv) {
	if (argc < 3){
		fprintf(stderr, "USAGE: acq400_chapi_siteclient_test UUT PORT [command]\n");
		exit(1);
	}
	const char* host = argv[1];
	int port = atoi(argv[2]);
	printf("connect to %s:%d\n", host, port);

	acq400_chapi::Siteclient site(host, port);
	if (argc > 3){
		for (int ic = 3; ic < argc; ++ic){
			char rx_message[80];
			int rc = site.sr(rx_message, 80, argv[ic]);
			if (rc > 0){
				printf("result %s\n", rx_message);
			}else{
				fprintf(stderr, "ERROR: sr returned %d\n", rc);
			}
		}
	}else{
		std::string tx_message;
		char rx_message[4096];

		while(std::getline(std::cin, tx_message)){
			int rc = site.sr(rx_message, 4096, "%s\n", tx_message.c_str());
			if (rc > 0){
				printf("result %s\n", rx_message);
			}else{
				fprintf(stderr, "ERROR: sr returned %d\n", rc);
			}
		}
	}
	return 0;
}
