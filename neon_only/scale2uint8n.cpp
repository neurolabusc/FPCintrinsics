// g++ -c -O3 scale2uint8n.cpp -o scale2uint8n.o
#include <arm_neon.h>
#include <stdio.h>
#include <stdlib.h>

void f32_i8neon(float *in32, uint8_t *out8, int64_t n, float slope1, float intercept1) {
	//bankers' rounding 
	float * vin = (float *)in32;
	uint8_t * vout = (uint8_t *)out8;
	float32x4_t intercept = vdupq_n_f32(intercept1);
	float32x4_t slope = vdupq_n_f32(slope1);
	#define kLoad 4 //we will load 4 floats (float32x4_t) 128 bits
	#define kStore 16 //we will store 16 bytes (uint8x16_t) 128 bits 
	if (n >= kStore) {
		for (int64_t i = 0; i <= (n-kStore); i+=kStore) {
			float32x4_t s0, s1;
			uint32x4_t i01, i23;
			s0 = vfmaq_f32 (intercept, slope, vld1q_f32(vin));
			vin += kLoad; //scaled 4 floats
			s1 = vfmaq_f32 (intercept, slope, vld1q_f32(vin));
			vin += kLoad; //scaled 4 floats
			i01 = vcombine_s16(vqmovn_s32(vcvtnq_s32_f32(s0)), vqmovn_s32(vcvtnq_s32_f32(s1))); 
			s0 = vfmaq_f32 (intercept, slope, vld1q_f32(vin));
			vin += kLoad; //scaled 4 floats
			s1 = vfmaq_f32 (intercept, slope, vld1q_f32(vin));
			vin += kLoad; //scaled 4 floats
			i23 = vcombine_s16(vqmovn_s32(vcvtnq_s32_f32(s0)), vqmovn_s32(vcvtnq_s32_f32(s1))); 
			uint8x16_t i01234 = vcombine_u8(vqmovun_s16(i01), vqmovun_s16(i23));
			vst1q_u8(vout, i01234);
			vout += kStore; //4x int32_t* -> store 128bits (16*uint8)
		}
	}
	int tail = (n % kStore);
	while (tail > 0) {
		int i = n - tail;
		float v = (in32[i] * slope1) + intercept1;
		v = fmin(255.0,v);
		v = fmax(0.0, v);
		out8[i] = lrint(v);
		tail --;	
	}
} //f32_i8neon()
