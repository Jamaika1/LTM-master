// Hook used by LCEVC TM to poke a stream into base decoder (avoiding filesystem)
//
#ifndef _INJECT_ANNEXB_H_
#define _INJECT_ANNEXB_H_

#ifdef __cplusplus
extern "C" {
#endif

extern void inject_annex_b_bitstream(byte *data, unsigned size);

#ifdef __cplusplus
}
#endif

#endif

