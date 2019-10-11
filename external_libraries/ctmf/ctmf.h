#ifndef CTMF_H
#define CTMF_H

#ifdef __cplusplus
extern "C" {
#endif


void ctmf(
	const unsigned char* src, unsigned char* dst,
	const int width, const int height,
	const int src_step_row, const int dst_step_row,
	const int r, const int channels, const unsigned long  memsize
);


void ctmf_orig(
        const unsigned char* src, unsigned char* dst,
        int width, int height,
        int src_step_row, int dst_step_row,
        int r, int channels, unsigned long memsize
        );


#ifdef __cplusplus
}
#endif

#endif
