/*
 * acq400_stream_awg.cpp
 *
 *  Created on: 3 September 2026
 *
 *  USAGE: acq400_stream_awg UUT1[:PORT1],UUT2[:PORT2]... [repeat|reps] [file ...]
 *      Author: pgm, SR
 */

#include "acq400_chapi.h"
#include "acq400_chapi_inc.h"
#include <thread>
#include <vector>
#include <string>

#define BUFLEN 0x10000

struct FileEntry {
        std::string name;
        FILE* fp;
};

typedef std::vector<FileEntry> FPV;

int G_reps = 0;

template <class C>
int streamer(const std::string& host, acq400_chapi::Acq400& uut, acq400_chapi::Ports port, const FPV& files, bool repeat)
{
        C* buf = new C[BUFLEN];
        int nbuf;
        int skt = 0;
        int reps = 0;

        do {
                for (const auto& fe : files){
                        FILE* fp = fopen(fe.name.c_str(), "rb");
                        if (!fp && fe.name == "stdin") fp = stdin;

                        if (fp) {
                                fprintf(stderr, "[%s] streaming file: %s\n", host.c_str(), fe.name.c_str());
                                while ((nbuf = fread(buf, sizeof(C), BUFLEN, fp)) > 0){
                                        uut.stream_out(&skt, buf, nbuf, port);
                                }
                                if (fe.name != "stdin") fclose(fp);
                        }
                }
                if (++reps > G_reps && G_reps){
                        break;
                }
        } while(repeat);

        delete[] buf;
        close(skt);
        return 0;
}

void worker_thread(std::string target_spec, const FPV& files, bool repeat) {
        acq400_chapi::Ports port = acq400_chapi::AWG_STREAM;
        std::vector<std::string> host_port;
        split(target_spec.c_str(), ':', host_port);

        std::string host = host_port[0];
        if (host_port.size() > 1){
                port = static_cast<acq400_chapi::Ports>(atoi(host_port[1].c_str()));
        }

        try {
                acq400_chapi::Acq400 uut(host.c_str());
                uut.wait_for_ready();

                int data32 = 0;
                if (uut.get("0", "data32", data32) < 0){
                        fprintf(stderr, "ERROR on UUT %s: failed to get data32\n", host.c_str());
                        return;
                }

                if (data32) {
                        streamer<long>(host, uut, port, files, repeat);
                } else {
                        streamer<short>(host, uut, port, files, repeat);
                }
        } catch (const std::exception& e) {
                fprintf(stderr, "Exception on UUT %s: %s\n", host.c_str(), e.what());
        }
}

#define USAGE "USAGE: acq400_stream_awg UUT1[:PORT1],UUT2[:PORT2]... [repeat|reps] [file ...]\n"

int main(int argc, char **argv) {
        if (argc < 2){
                fprintf(stderr, USAGE);
                exit(1);
        }

        std::string uuts_arg = argv[1];

        FPV files;
        bool repeat = false;

        if (argc > 2){
                int i1 = 2;
                if (strcmp(argv[2], "repeat") == 0 || (G_reps = atoi(argv[2])) > 0){
                        if (argc > 3){
                                i1 = 3;
                                repeat = true;
                        } else {
                                fprintf(stderr, "ERROR: repeat only supported with file\n");
                                exit(1);
                        }
                }
                for (int ii = i1; ii < argc; ++ii){
                        const char* fname = argv[ii];
                        FILE* fp = fopen(fname, "rb");
                        if (fp == nullptr){
                                fprintf(stderr, "ERROR failed to open file \"%s\"\n", fname);
                                exit(1);
                        }
                        fclose(fp);
                        files.push_back({fname, nullptr});
                }
        } else {
                files.push_back({"stdin", stdin});
        }

        std::vector<std::string> uut_targets;
        split(uuts_arg.c_str(), ',', uut_targets);

        std::vector<std::thread> threads;
        threads.reserve(uut_targets.size());

        // Iterate through parsed UUT targets in reverse order
        for (auto it = uut_targets.rbegin(); it != uut_targets.rend(); ++it) {
                threads.emplace_back(worker_thread, *it, std::cref(files), repeat);
        }

        for (auto& t : threads) {
                if (t.joinable()) {
                        t.join();
                }
        }

        return 0;
}
