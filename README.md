## Introduction
Modern computers perform most tasks extremely quickly. However, some computationally expensive tasks can benefit from optimization. Three popular tricks for making slow computations faster are:

 - Use multiple threads to run the task in parallel on multiple central processing unit (CPU) cores.
 - Use a [compute shader](https://github.com/neurolabusc/Metal-Demos/tree/master/compute) to a exploit the massively parallel graphics processing unit (GPU) instead of the central processing unit.
 - Use [Single instruction, multiple data (SIMD)](https://en.wikipedia.org/wiki/SIMD) registers on a single CPU to compute multiple similar computations simultaneously.

Usually only a few core functions need to be written to leverage SIMD instructions: not all algortihms lend themselves to SIMD processing. Further, since SIMD is typically hard to write and maintain it makes sense to only port the most time critical functions to SIMD.

This page describes a method for inserting SIMD code into [FreePascal programs](https://www.freepascal.org). For example, every x86-64 (AMD, Intel) CPU supports the [Streaming SIMD Extensions (SSE)](https://en.wikipedia.org/wiki/Streaming_SIMD_Extensions) which process 128-bytes at a time (e.g. 4 32-bit single precision floating point numbers). Most ARM CPUs like those in recent Apple Macintosh M1 computers and the Raspberry Pi support the [128-bit Neon SIMD instructions](https://developer.arm.com/architectures/instruction-sets/simd-isas/neon).

C programmers can either support SIMD instructions directly by using assembly, or they can write traditional C but call  [Intrinsic functions for x86-64](https://software.intel.com/sites/landingpage/IntrinsicsGuide/) or [ARM](https://github.com/gcc-mirror/gcc/blob/master/gcc/config/arm/arm_neon.h) CPUs. Intrinsics have [benefits and costs](https://developer.arm.com/documentation/102467/0100/Why-Neon-Intrinsics-) relative to assembly. In brief, most programmers find intrinsics easier to write and port than assembly code. Modern compilers do a great job of optimizing intrinsics, so they typically perform equivalently to assembly for most cases.

Unfortunately, FreePascal does not provide SIMD intrinsics, so users are typically forced to write assembly code. This page describes an alternative: writing core functions using C intrinsics and inserting the compiled objects into Pascal projects. The only trick is knowing how your C compiler will rename your functions ([symbol mangling](http://web.mit.edu/tibbetts/Public/inside-c/www/mangling.html)). In the examples below the C function `f32_i8sse` is renamed  `__Z9f32_i8ssePfPaxff` when compiled on macOS and named `_Z9f32_i8ssePfPalff` on Linux. You can find the name of symbols with a command like `objdump -t scale2uint8.o`.

Here I demonstrate this process using MacOS and Linux, and both x86-64 and ARM aarch64 architectures. The Windows operating system might be different. Also, SSE and Neon [may require](https://lemire.me/blog/2012/05/31/data-alignment-for-speed-myth-or-reality/) arrays to be aligned on 128-bit boundaries. In my experience, this is the default for FreePascal on Linux and MacOS.

#### A Neon-only Example

Consider code that takes an array of 32-bit (single precision) floating point values and converts them to a clamped array of bytes (0..255) based on a slope and intercept. The scalar Pascal pseudocode might look like this:

```
	for i := 0 to (n-1) do
		outUI8[i] := max(min((inF32[i] * slope) + intercept, 255), 0);
```

For 128-bit Neon, we can store sixteen values for each interation using [four fused multiply-adds](https://docs.unity3d.com/Packages/com.unity.burst@1.6/api/Unity.Burst.Intrinsics.Arm.Neon.vfmaq_f32.html). The [Neon intrinsics](https://github.com/gcc-mirror/gcc/blob/master/gcc/config/arm/arm_neon.h) will look like this:

```
	#define kLoad 4 //we will load 4 floats (float32x4_t) 128 bits
	#define kStore 16 //we will store 16 bytes (uint8x16_t) 128 bits 
	for (int64_t i = 0; i <= (n-kStore); i+=kStore) {
		float32x4_t s0, s1;
		uint32x4_t i01, i23;
		s0 = vfmaq_f32 (intercept, slope, vld1q_f32(vin)); //fused multiply add
		vin += kLoad; //scaled 4 floats
		s1 = vfmaq_f32 (intercept, slope, vld1q_f32(vin)); //fused multiply add
		vin += kLoad; //scaled 4 floats
		i01 = vcombine_s16(vqmovn_s32(vcvtnq_s32_f32(s0)), vqmovn_s32(vcvtnq_s32_f32(s1))); //f32->i32
		s0 = vfmaq_f32 (intercept, slope, vld1q_f32(vin)); //fused multiply add
		vin += kLoad; //scaled 4 floats
		s1 = vfmaq_f32 (intercept, slope, vld1q_f32(vin)); //fused multiply add
		vin += kLoad; //scaled 4 floats
		i23 = vcombine_s16(vqmovn_s32(vcvtnq_s32_f32(s0)), vqmovn_s32(vcvtnq_s32_f32(s1))); //f32->i32
		uint8x16_t i01234 = vcombine_u8(vqmovun_s16(i01), vqmovun_s16(i23)); //s32->s16->u8
		vst1q_u8(vout, i01234); //store output
		vout += kStore; //4x int32_t* -> store 128bits (16*uint8)
	}
```

Compiling and running this Project on an Apple MacBook Air with an ARM aarch64 M1 CPU reveals a dramatic performance benefit:

```
$ g++ -c -O3 scale2uint8n.cpp -o scale2uint8n.o
$ fpc -O3 simdn.pas; ./simdn                   
Free Pascal Compiler version 3.3.1 [2021/04/19] for aarch64
Copyright (c) 1993-2021 by Florian Klaempfl and others
Target OS: Darwin for AArch64
Compiling simdn.pas
Assembling (pipe) simdn.s
Linking simdn
67 lines compiled, 0.1 sec
values 269568000 repetitions 3
f32 elapsed SIMD (msec) min 24 total 89
f32 elapsed FPC (msec) min 435 total 1322
```

#### A Neon and SSE Example

One challenge is that x86-64 CPUs SIMD use the SSE and AVX instruction sets while ARM CPUs use the Neon (and in future SVE) instruction sets. Therefore, one typically must write separate code for these two architectures. Alternatively, one could simply write SSE intrinsics and use the [sse2neon](https://github.com/DLTcollab/sse2neon) when compiling to Neon (or code Neon and use [this library](https://github.com/intel/ARM_NEON_2_x86_SSE) when compiling to x86-64). Note that Neon and SSE are not [bijective, meaning there is no exact one to one map](http://codesuppository.blogspot.com/2015/02/sse2neonh-porting-guide-and-header-file.html). However, for most cases this allows one to choose one architecture and generate remarkably efficient SIMD code for another architecture. A secondary benefit is that the Neon intrinsics are poorly documented, unlike the [x86-64 intrinsic functions](https://software.intel.com/sites/landingpage/IntrinsicsGuide/). Further, a simple web search will often reveal an example of an open source x86-64 intrinsics that solves your problem, while example Neon code is much less common. 

Here is an example code of SSE code ported to Neon on an Apple aarch64-base M1:

```
$ g++ -c -O3 scale2uint8.cpp -o scale2uint8.o 
$ fpc -O3 simd.pas; ./simd                  
Free Pascal Compiler version 3.3.1 [2021/04/19] for aarch64
Copyright (c) 1993-2021 by Florian Klaempfl and others
Target OS: Darwin for AArch64
Compiling simd.pas
Assembling (pipe) simd.s
Linking simd
69 lines compiled, 0.2 sec
values 269568000 repetitions 3
f32 elapsed C (msec) min 25 total 96
f32 elapsed FPC (msec) min 445 total 1344
```

We can compile the same code on an AMD Ryzen 3900X running Ubuntu 20.04:

```
$ g++  -mfma  -c -O3 scale2uint8.cpp -o scale2uint8.o 
$ fpc -O3 simd.pas; ./simd   
Free Pascal Compiler version 3.2.0 [2020/07/07] for x86_64
Copyright (c) 1993-2020 by Florian Klaempfl and others
Target OS: Linux for x86-64
Compiling simd.pas
Linking simd
67 lines compiled, 0.1 sec
values 269568000 repetitions 3
f32 elapsed C (msec) min 54 total 162
f32 elapsed FPC (msec) min 639 total 1919
```

#### Alternatives

There are a few handy SIMD libraries for Pascal that use SIMD. These provide easy access to using these functions. However, most of these routines are general purpose and may not be as efficient as writing your own code. 

 - [VectorMath](https://github.com/jdelauney/SIMD-VectorMath-UnitTest).
 - [oprsimd](https://github.com/zamronypj/oprsimd)
 - [SIMD project](https://github.com/zamronypj/simd) for Lazarus.
 - [mrmath](https://github.com/mikerabat/mrmath).
 - [DirectXMath](https://github.com/CMCHTPC/DirectXMath).
 - [Synopse mORMot framework](https://github.com/synopse/mORMot) includes some SIMD accelerated functions.
 - [Exentia](http://www.tommesani.com/ExentiaWhatsNew.html) SIMD for Delphi.
 - [FastMath](https://github.com/neslib/FastMath) for Delphi.
