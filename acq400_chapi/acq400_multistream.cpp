/*
 * acq400_multistream.cpp
 *
 *  Created on: 10 September 2021
 *
 *  USAGE: acq400_multistream UUT1[:PORT1],UUT2[:PORT2]... [DEFAULT_PORT] [--dir OUT_DIR]
 *      Author: SR
 */

#include "acq400_chapi.h"
#include "acq400_chapi_inc.h"
#include <thread>
#include <vector>
#include <string>
#include <cassert>
#include <sys/stat.h>
#include <sys/types.h>

#define BUFLEN 0x10000

namespace G {
        unsigned long long maxbytes = 0;
};

// Helper function to create directories recursively (mkdir -p equivalent)
void make_path(const std::string& path) {
        std::string current_level = "";
        std::stringstream ss(path);
        std::string section;

        while (std::getline(ss, section, '/')) {
                if (section.empty()) {
                        current_level += "/";
                        continue;
                }
                current_level += section + "/";
#if defined(_WIN32)
                mkdir(current_level.c_str());
#else
                mkdir(current_level.c_str(), 0777);
#endif
        }
}

template <class C>
int streamer(const std::string& host, acq400_chapi::Acq400& uut, acq400_chapi::Ports port, FILE* out_fp)
{
        C* buf = new C[BUFLEN];
        int nbuf;
        unsigned long long nbytes = 0;

        while ((nbuf = uut.stream(buf, BUFLEN, port)) > 0){
                fwrite(buf, sizeof(C), nbuf, out_fp);
                nbytes += nbuf * sizeof(C);
                if (G::maxbytes && nbytes >= G::maxbytes){
                        break;
                }
        }

        delete[] buf;
        return 0;
}

void set_data_mode(acq400_chapi::Acq400& uut, int pack24)
{
        int rc = uut.set("1", "pack24", pack24);
        assert(rc == 0);
}

#define ENABLE 1
#define RISING 1

void set_trigger_mode(acq400_chapi::Acq400& uut, int soft_trigger)
{
        std::string response;
        int rc = uut.set(response, "1", "trg=%d,%d,%d", ENABLE, soft_trigger, RISING);
        assert(rc == 0);
}

void set_pack24_if_available_and_requested(acq400_chapi::Acq400& uut){
        int pack24;
        if (uut.get("1", "pack24", pack24) == acq400_chapi::RC_SUCCESS){
                pack24 = 0;
                if (getenv("ACQ400_PACK24")){
                        pack24 = atoi(getenv("ACQ400_PACK24"));
                }
                set_data_mode(uut, pack24);
        }
}

void set_uut_mode(acq400_chapi::Acq400& uut){
        set_pack24_if_available_and_requested(uut);

        int soft_trigger = 0;
        if (getenv("ACQ400_SOFT_TRIGGER")){
                soft_trigger = atoi(getenv("ACQ400_SOFT_TRIGGER"));
        }

        set_trigger_mode(uut, soft_trigger);
}

void worker_thread(std::string target_spec, acq400_chapi::Ports default_port, std::string base_dir) {
        acq400_chapi::Ports port = default_port;
        std::vector<std::string> host_port;
        split(target_spec.c_str(), ':', host_port);

        std::string host = host_port[0];
        if (host_port.size() > 1){
                port = static_cast<acq400_chapi::Ports>(atoi(host_port[1].c_str()));
        }

        // Build path compatible with host_demux expectation: <base_dir>/<uut_name>/0000
        std::string uut_dir = base_dir + "/" + host;
        make_path(uut_dir);
        
        std::string fname = uut_dir + "/0000";
        FILE* out_fp = fopen(fname.c_str(), "wb");
        if (!out_fp) {
                fprintf(stderr, "ERROR [%s]: Unable to open output file %s\n", host.c_str(), fname.c_str());
                return;
        }

        fprintf(stderr, "[%s] Streaming output to file: %s\n", host.c_str(), fname.c_str());

        try {
                acq400_chapi::Acq400 uut(host.c_str());
                uut.wait_for_ready();

                set_uut_mode(uut);

                int data32 = 0;
                if (uut.get("0", "data32", data32) < 0){
                        fprintf(stderr, "ERROR [%s]: Failed to get data32\n", host.c_str());
                        fclose(out_fp);
                        return;
                }

                if (data32){
                        streamer<long>(host, uut, port, out_fp);
                } else {
                        streamer<short>(host, uut, port, out_fp);
                }
        } catch (const std::exception& e) {
                fprintf(stderr, "Exception [%s]: %s\n", host.c_str(), e.what());
        }

        fclose(out_fp);
}

#define USAGE "USAGE: acq400_multistream UUT1[:PORT1],UUT2[:PORT2]... [DEFAULT_PORT] [OUT_DIR]\n"

int main(int argc, char **argv) {
        if (argc < 2){
                fprintf(stderr, USAGE);
                exit(1);
        }

        if (getenv("ACQ400_STREAM_MAXB")){
                G::maxbytes = strtoull(getenv("ACQ400_STREAM_MAXB"), 0, 0);
        }
        if (getenv("ACQ400_STREAM_MAXM")){
                G::maxbytes = strtoull(getenv("ACQ400_STREAM_MAXM"), 0, 0) * 0x100000;
        }

        std::string uuts_arg = argv[1];
        acq400_chapi::Ports default_port = acq400_chapi::STREAM;
        std::string base_dir = "./DATA";

        if (argc > 2){
                // Check if second argument is port number or custom base output directory
                if (isdigit(argv[2][0])) {
                        default_port = static_cast<acq400_chapi::Ports>(atoi(argv[2]));
                        if (argc > 3) {
                                base_dir = argv[3];
                        }
                } else {
                        base_dir = argv[2];
                }
        }

        std::vector<std::string> uut_targets;
        split(uuts_arg.c_str(), ',', uut_targets);

        std::vector<std::thread> threads;
        threads.reserve(uut_targets.size());

        for (const auto& target : uut_targets) {
                threads.emplace_back(worker_thread, target, default_port, base_dir);
        }

        for (auto& t : threads) {
                if (t.joinable()) {
                        t.join();
                }
        }

        return 0;
}
