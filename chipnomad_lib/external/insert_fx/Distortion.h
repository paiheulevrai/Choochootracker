// DSP extracted from Airwindows; MIT license in LICENSE-Airwindows.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
namespace insert_dsp {
class Distortion {
public:


	double previousInL[9];
	double previousOutL[9];
	double previousInR[9];
	double previousOutR[9];

	uint32_t fpdL;
	uint32_t fpdR;
	//default stuff

    float A;
    float B;
    float C;
    float D;

 double rate = 48000;
 double cachedA=-999, cachedC=-999, gainA=1, gainC=1;
 double getSampleRate() const {return rate;}
 void reset(double sampleRate) {
 memset(this, 0, sizeof(*this)); rate=sampleRate; cachedA=cachedC=-999;

	A = 0.5;
	B = 0.5;
	C = 0.5;
	D = 1.0;
	for (int x = 0; x < 9; x++) {
		previousInL[x] = 0.0; previousOutL[x] = 0.0;
		previousInR[x] = 0.0; previousOutR[x] = 0.0;
	}
	fpdL = 0x12345678u;
	fpdR = 0x87654321u;
	//this is reset: values being initialized only once. Startup values, whatever they are.


}
 void processReplacing(float**, float**, int);
};
/* ========================================
 *  Distortion - Distortion.h
 *  Copyright (c) 2016 airwindows, Airwindows uses the MIT license
 * ======================================== */



void Distortion::processReplacing(float **inputs, float **outputs, int sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	int stages = (int)floor(getSampleRate()/25000.0);
	if (stages > 8) stages = 8;
	if (cachedA != A) { cachedA=A; gainA=pow(10.0,((A-0.5)*24.0)/20.0); }
 double input=gainA;
	int mode = (int) ( B * 4.999 );
	if (cachedC != C) { cachedC=C; gainC=pow(10.0,((C-0.5)*24.0)/20.0); }
 double output=gainC;
	double wet = D;

    while (--sampleFrames >= 0)
    {
		double inputSampleL = *in1;
		double inputSampleR = *in2;
		if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
		if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;
		double drySampleL = inputSampleL;
		double drySampleR = inputSampleR;

		inputSampleL *= input;
		inputSampleR *= input;

		for (int x = 0; x < stages; x++) {
			double temp;
			temp = (inputSampleL+previousInL[x])*0.5;
			previousInL[x] = inputSampleL;
			inputSampleL = temp;
			temp = (inputSampleR+previousInR[x])*0.5;
			previousInR[x] = inputSampleR;
			inputSampleR = temp;
		}

		switch (mode)
		{
			case 0: //Density
				if (inputSampleL > 1.570796326794897) inputSampleL = 1.570796326794897;
				if (inputSampleL < -1.570796326794897) inputSampleL = -1.570796326794897;
				if (inputSampleR > 1.570796326794897) inputSampleR = 1.570796326794897;
				if (inputSampleR < -1.570796326794897) inputSampleR = -1.570796326794897;
				//clip to 1.570796326794897 to reach maximum output
				inputSampleL = sin(inputSampleL);
				inputSampleR = sin(inputSampleR);
				break;
			case 1: //Drive
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				inputSampleL -= (inputSampleL * (fabs(inputSampleL) * 0.6) * (fabs(inputSampleL) * 0.6));
				inputSampleR -= (inputSampleR * (fabs(inputSampleR) * 0.6) * (fabs(inputSampleR) * 0.6));
				inputSampleL *= 1.5;
				inputSampleR *= 1.5;
				break;
			case 2: //Spiral
				if (inputSampleL > 1.2533141373155) inputSampleL = 1.2533141373155;
				if (inputSampleL < -1.2533141373155) inputSampleL = -1.2533141373155;
				if (inputSampleR > 1.2533141373155) inputSampleR = 1.2533141373155;
				if (inputSampleR < -1.2533141373155) inputSampleR = -1.2533141373155;
				//clip to 1.2533141373155 to reach maximum output
				inputSampleL = sin(inputSampleL * fabs(inputSampleL)) / ((fabs(inputSampleL) == 0.0) ?1:fabs(inputSampleL));
				inputSampleR = sin(inputSampleR * fabs(inputSampleR)) / ((fabs(inputSampleR) == 0.0) ?1:fabs(inputSampleR));
				break;
			case 3: //Mojo
				double mojo; mojo = pow(fabs(inputSampleL),0.25);
				if (mojo > 0.0) inputSampleL = (sin(inputSampleL * mojo * M_PI * 0.5) / mojo) * 0.987654321;
				mojo = pow(fabs(inputSampleR),0.25);
				if (mojo > 0.0) inputSampleR = (sin(inputSampleR * mojo * M_PI * 0.5) / mojo) * 0.987654321;
				//mojo is the one that flattens WAAAAY out very softly before wavefolding
				break;
			case 4: //Dyno
				double dyno; dyno = pow(fabs(inputSampleL),4);
				if (dyno > 0.0) inputSampleL = (sin(inputSampleL * dyno) / dyno) * 1.1654321;
				dyno = pow(fabs(inputSampleR),4);
				if (dyno > 0.0) inputSampleR = (sin(inputSampleR * dyno) / dyno) * 1.1654321;
				//dyno is the one that tries to raise peak energy
				break;
		}

		for (int x = 1; x < (stages/2); x++) {
			double temp;
			temp = (inputSampleL+previousOutL[x])*0.5;
			previousOutL[x] = inputSampleL;
			inputSampleL = temp;
			temp = (inputSampleR+previousOutR[x])*0.5;
			previousOutR[x] = inputSampleR;
			inputSampleR = temp;
		}

		if (output != 1.0) {
			inputSampleL *= output;
			inputSampleR *= output;
		}

		if (wet !=1.0) {
			inputSampleL = (inputSampleL * wet) + (drySampleL * (1.0-wet));
			inputSampleR = (inputSampleR * wet) + (drySampleR * (1.0-wet));
		}

		//begin 32 bit stereo floating point dither
		int expon; frexpf((float)inputSampleL, &expon);
		fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
		inputSampleL += ((double(fpdL)-uint32_t(0x7fffffff)) * 5.5e-36 * ldexp(1.0,expon+62));
		frexpf((float)inputSampleR, &expon);
		fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
		inputSampleR += ((double(fpdR)-uint32_t(0x7fffffff)) * 5.5e-36 * ldexp(1.0,expon+62));
		//end 32 bit stereo floating point dither

		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}


}
