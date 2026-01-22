// #pragma once
//
// #include <xmmintrin.h>
// #include <cfenv>
// #include <fenv.h>
// #include <float.h>
// #include <pmmintrin.h>
//
//
// /**
//  *    To be called at the start of each tread which has to have consistent floating point operations.
//  */
// inline void set_fpu_friendly_mode() {
//     // 1. SSE: flush denormals to zero
//     unsigned int mxcsr = _mm_getcsr();
//     mxcsr |= _MM_FLUSH_ZERO_ON;
//     mxcsr |= _MM_DENORMALS_ZERO_ON;
//     _mm_setcsr(mxcsr);
//
//     unsigned int cw;
//     _controlfp_s(&cw, _PC_53, _MCW_PC);
//     _controlfp_s(&cw, _RC_NEAR, _MCW_RC);
//
//
//     fesetround(FE_TONEAREST);
// }