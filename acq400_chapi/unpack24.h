/*
 * unpack24.h
 *
 * Unpacks 24-bit data back to original 32-bit pattern with channel id.
 * Supports both byte-by-byte (Unpack24b) and longword-optimized (Unpack24lw) unpacking.
 */

#ifndef ACQ400_UNPACK24_H_
#define ACQ400_UNPACK24_H_

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <memory>
#include <vector>

inline unsigned ch_id(int ic)
{
	unsigned site = ic / 32 + 1;
	return (unsigned)ic | (site << 5);
}

class Unpack24 {
protected:
	const int nchan;
	const int nspad;
public:
	Unpack24(int _nchan, int _nspad): 
		nchan(_nchan), nspad(_nspad)
	{}
	virtual ~Unpack24() {}
	virtual int unpack24(FILE* fin, FILE* fout, unsigned long long maxbytes = 0) = 0;

	static std::unique_ptr<Unpack24> create(int nchan, int nspad);
	static Unpack24& factory();
};

class Unpack24b: public Unpack24 {
public:
	Unpack24b(int _nchan, int _nspad): 
		Unpack24(_nchan, _nspad) 
	{}

	virtual int unpack24(FILE* fin, FILE* fout, unsigned long long maxbytes = 0) override;
};

class Unpack24lw: public Unpack24 {
public:
	Unpack24lw(int _nchan, int _nspad): 
		Unpack24(_nchan, _nspad) 
	{}

	virtual int unpack24(FILE* fin, FILE* fout, unsigned long long maxbytes = 0) override;
};

inline int Unpack24b::unpack24(FILE* fin, FILE* fout, unsigned long long maxbytes)
{
	const int BUFFER_SCANS = 1024;
	const int issb = nchan * 3 + nspad * 4;
	const int ossw = nchan + nspad;
	std::vector<unsigned char> ibuf(BUFFER_SCANS * issb);
	std::vector<unsigned> obuf(BUFFER_SCANS * ossw);

	unsigned long long total_out_bytes = 0;
	size_t scans_read;

	while ((scans_read = fread(ibuf.data(), issb, BUFFER_SCANS, fin)) > 0) {
		const unsigned char* ip = ibuf.data();
		unsigned* op = obuf.data();

		for (size_t s = 0; s < scans_read; ++s) {
			for (int ic = 0; ic < nchan; ++ic) {
				unsigned char b1 = *ip++;
				unsigned char b2 = *ip++;
				unsigned char b3 = *ip++;
				*op++ = ((unsigned)b1 << 8) | ((unsigned)b2 << 16) | ((unsigned)b3 << 24) | ch_id(ic);
			}
			for (int isp = 0; isp < nspad; ++isp) {
				unsigned spad_word;
				memcpy(&spad_word, ip, sizeof(unsigned));
				ip += sizeof(unsigned);
				*op++ = spad_word;
			}
		}

		size_t words_to_write = scans_read * ossw;
		if (maxbytes > 0) {
			unsigned long long bytes_to_write = words_to_write * sizeof(unsigned);
			if (total_out_bytes + bytes_to_write > maxbytes) {
				words_to_write = (size_t)((maxbytes - total_out_bytes) / sizeof(unsigned));
			}
		}

		if (words_to_write > 0) {
			if (fwrite(obuf.data(), sizeof(unsigned), words_to_write, fout) != words_to_write) {
				return -1;
			}
			total_out_bytes += words_to_write * sizeof(unsigned);
		}

		if (maxbytes > 0 && total_out_bytes >= maxbytes) {
			break;
		}
	}
	return 0;
}

#define AAS 24
#define BBS 16
#define CCS  8
#define DDS  0

#define AA (0xFFU<<AAS)
#define BB (0xFFU<<BBS)
#define CC (0xFFU<<CCS)
#define DD (0xFFU<<DDS)

inline int Unpack24lw::unpack24(FILE* fin, FILE* fout, unsigned long long maxbytes)
{
	const int BUFFER_SCANS = 1024;
	const int issw_d24 = nchan * 3 / 4;
	const int issw = issw_d24 + nspad;
	const int ossw = nchan + nspad;

	std::vector<unsigned> ibuf(BUFFER_SCANS * issw);
	std::vector<unsigned> obuf(BUFFER_SCANS * ossw);

	unsigned long long total_out_bytes = 0;
	size_t scans_read;

	while ((scans_read = fread(ibuf.data(), issw * sizeof(unsigned), BUFFER_SCANS, fin)) > 0) {
		const unsigned* ip = ibuf.data();
		unsigned* op = obuf.data();

		for (size_t s = 0; s < scans_read; ++s) {
			const unsigned* ibp = ip;
			int ic = 0;
			for (int iw = 0; iw < issw_d24; iw += 3, ic += 4) {
				op[0] = ((ibp[iw + 0] & (BB | CC | DD)) <<  8)                                          | ch_id(ic + 0);
				op[1] = ((ibp[iw + 0] & AA) >> 16)             | ((ibp[iw + 1] & (CC | DD)) << 16)     | ch_id(ic + 1);
				op[2] = ((ibp[iw + 1] & (AA | BB)) >> 8)      | ((ibp[iw + 2] & DD) << 24)            | ch_id(ic + 2);
				op[3] = (ibp[iw + 2] & (AA | BB | CC))                                                 | ch_id(ic + 3);
				op += 4;
			}
			for (int iw = 0; iw < nspad; ++iw) {
				*op++ = ibp[issw_d24 + iw];
			}
			ip += issw;
		}

		size_t words_to_write = scans_read * ossw;
		if (maxbytes > 0) {
			unsigned long long bytes_to_write = words_to_write * sizeof(unsigned);
			if (total_out_bytes + bytes_to_write > maxbytes) {
				words_to_write = (size_t)((maxbytes - total_out_bytes) / sizeof(unsigned));
			}
		}

		if (words_to_write > 0) {
			if (fwrite(obuf.data(), sizeof(unsigned), words_to_write, fout) != words_to_write) {
				return -1;
			}
			total_out_bytes += words_to_write * sizeof(unsigned);
		}

		if (maxbytes > 0 && total_out_bytes >= maxbytes) {
			break;
		}
	}
	return 0;
}

inline std::unique_ptr<Unpack24> Unpack24::create(int nchan, int nspad)
{
	if (nchan % 4 == 0) {
		return std::make_unique<Unpack24lw>(nchan, nspad);
	} else {
		return std::make_unique<Unpack24b>(nchan, nspad);
	}
}

#endif /* ACQ400_UNPACK24_H_ */
