program simdn;
//Example of how to insert C code into FPC project
// in this example, the C code uses SIMD intrinsics to generate SSE or Neon code
//you must compile scale2uint8n.cpp first!
// g++ -c -O3 scale2uint8n.cpp -o scale2uint8n.o
// fpc -O3 simdn.pas; ./simdn

{$mode objfpc}{$H+} 
uses Math, SysUtils,DateUtils;

{$L scale2uint8n.o}
function f32_i8neon(in32: pointer; out8: pointer; n: int64; slope, intercept: single): Integer; external name '__Z10f32_i8neonPfPhxff';

procedure testF32(reps: integer = 3);
const
  //number of voxels for test, based on HCP resting state  https://protocols.humanconnectome.org/HCP/3T/imaging-protocols.html
  n = 104*90*72*400; //= 104*90*72*400;
  slope = 1;
  intercept = 0.5;
var
  i, r: Integer;
  f: single;
  in32: array of single;
  out8fpc, out8c: array of byte;
  startTime : TDateTime;
  ms, cSum, fpcSum, cMin, fpcMin: Int64;
begin
	//initialize
	Writeln('values ', n, ' repetitions ', reps);
	setlength(in32, n);
	setlength(out8c, n);
	setlength(out8fpc, n);
	cSum := 0;
	fpcSum := 0;
	cMin := MaxInt;
	fpcMin := MaxInt;
	for i := 0 to (n-1) do
		in32[i] := (random(2048) - 100) * 0.1;
	for r := 1 to (reps) do begin
		//c Code
		startTime := Now;
		f32_i8neon(@in32[0], @out8c[0], n, slope, intercept);
		ms := MilliSecondsBetween(Now,startTime);
		cMin := min(cMin, ms);
		cSum += ms;
		//fpc code:
		startTime := Now;
		for i := 0 to (n-1) do begin
			f := max(min((in32[i] * slope) + intercept, 255), 0); 
			out8fpc[i] := round(f);
			end;
			ms := MilliSecondsBetween(Now,startTime);
			fpcMin := min(fpcMin, ms);
			fpcSum += ms;
	end;
	//validate results:
	for i := 0 to (n-1) do begin
		if (out8c[i] <> out8fpc[i]) then
			Writeln(i, ' ', in32[i], ' c->', out8c[i], ' fpc-> ', out8fpc[i]);
	end;
	Writeln('f32 elapsed SIMD (msec) min ', cMin, ' total ', cSum);
	Writeln('f32 elapsed FPC (msec) min ', fpcMin, ' total ', fpcSum);
end; //testF32()

begin
	testF32(3);
end.
