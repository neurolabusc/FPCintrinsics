// g++ -c -O3 scale2uint8.cpp -o scale2uint8.o

#include <stdio.h>
#ifdef __x86_64__    
	#include <immintrin.h>
#else  
	#include "sse2neon.h"
#endif 

#ifndef MAX
#define MAX(A,B) ((A) > (B) ? (A) : (B))
#endif

#ifndef MIN
#define MIN(A,B) ((A) > (B) ? (B) : (A))
#endif

#ifndef lrint
long lrint( double x ){
    return x < 0 ? x - 0.5 : x + 0.5;
}
#endif


#define kSSE 4 //128-bit SSE handles 4 32-bit floats per instruction

void f32_i8sse(float *in32, int8_t *out8, int64_t n, float slope1, float intercept1) {
	//bankers' rounding 
	float * vin = (float *)in32;
	__m128i * vout = (__m128i *)out8;
	__m128 intercept = _mm_set1_ps(intercept1);
	__m128 slope = _mm_set1_ps(slope1);
	#define kStep 16
	if (n >= kStep) {
		for (int64_t i = 0; i <= (n-kStep); i+=kStep) {
			//we will save 16 bytes (128 bits) per iteration
			__m128 v4, s0, s1, s2, s3;
			__m128i i0, i1, i01, i23;
			v4 = _mm_load_ps(vin); //read 4*float32
			s0 = _mm_fmadd_ps (v4, slope, intercept);
			vin += kSSE; //scaled 4 floats
			v4 = _mm_load_ps(vin); //read 4*float32
			s1 = _mm_fmadd_ps (v4, slope, intercept);
			vin += kSSE; //scaled 4 floats
			i0 = _mm_cvtps_epi32(s0); //4*float32 -> 4*int32
			i1 = _mm_cvtps_epi32(s1); //4*float32 -> 4*int32
			i01 = _mm_packs_epi32 (i0, i1);	//4*int32,4xint32 -> 8*int16	
			v4 = _mm_load_ps(vin); //read 4*float32
			s0 = _mm_fmadd_ps (v4, slope, intercept);
			vin += kSSE; //scaled 4 floats
			v4 = _mm_load_ps(vin); //read 4*float32
			s1 = _mm_fmadd_ps (v4, slope, intercept);
			vin += kSSE; //scaled 4 floats
			i0 = _mm_cvtps_epi32(s0); //4*float32 -> 4*int32
			i1 = _mm_cvtps_epi32(s1); //4*float32 -> 4*int32
			i23 = _mm_packs_epi32 (i0, i1); //4*int32,4xint32 -> 8*int16
			__m128i i01234 = _mm_packus_epi16 (i01, i23); // 8*int16,8*int16 -> 16*uint8
			_mm_store_si128(vout, i01234); 
			vout += 1; //store 128bits (16*uint8)
		}
	}
	int tail = (n % kStep);
	while (tail > 0) {
		int i = n - tail;
		float v = (in32[i] * slope1) + intercept1;
		v = MIN(255.0,v);
		v = MAX(0.0, v);
		out8[i] = lrint(v);
		tail --;	
	}	
}
