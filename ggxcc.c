#define SCHED_IMPLEMENTATION
#include "mm_sched.h"
#define JRC_DDS_IMPLEMENTATION
#include "jrc_dds.h"
#define RYG_SRGB_CONV_IMPLEMENTATION
#include "ryg_srgb_conv.h"

#include <stdint.h>

// ***************************************************************************
// jrc_time.h

#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

int64_t jrcGetTime()
{
#ifdef WIN32
	static int64_t ticksPerSec = 0;
	LARGE_INTEGER pc;

	if (!ticksPerSec)
	{
		LARGE_INTEGER freq;
		QueryPerformanceFrequency(&freq);
		ticksPerSec = freq.QuadPart;
	}

	QueryPerformanceCounter(&pc);

	return pc.QuadPart * 1000l / ticksPerSec;
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	
	return (int64_t)tv.tv_usec / 1000l + (int64_t)tv.tv_sec * 1000l;
#endif
}

// ***************************************************************************

/*
      major axis
      direction     target                             sc     tc    ma
      ----------    -------------------------------    ---    ---   ---
       +rx          TEXTURE_CUBE_MAP_POSITIVE_X_ARB    -rz    -ry   rx
       -rx          TEXTURE_CUBE_MAP_NEGATIVE_X_ARB    +rz    -ry   rx
       +ry          TEXTURE_CUBE_MAP_POSITIVE_Y_ARB    +rx    +rz   ry
       -ry          TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB    +rx    -rz   ry
       +rz          TEXTURE_CUBE_MAP_POSITIVE_Z_ARB    +rx    -ry   rz
       -rz          TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB    -rx    -ry   rz

        s   =   ( sc/|ma| + 1 ) / 2
        t   =   ( tc/|ma| + 1 ) / 2
*/

// ***************************************************************************
// Minimal vector lib

typedef float vec2_t[2];
typedef float vec3_t[3];

#define Vec3Set(o, a, b, c) ((o)[0] = (a), (o)[1] = (b), (o)[2] = (c))
#define Vec3Scale(o, s, v) ((o)[0] = (s) * (v)[0], (o)[1] = (s) * (v)[1], (o)[2] = (s) * (v)[2])
#define Vec3Add(o, a, b) ((o)[0] = (a)[0] + (b)[0], (o)[1] = (a)[1] + (b)[1], (o)[2] = (a)[2] + (b)[2])
#define DotProduct(a, b) ((a)[0] * (b)[0] + (a)[1] * (b)[1] + (a)[2] * (b)[2])

#define CLAMP(i, min, max) (((i) < (min)) ? (min) : ((i) > (max)) ? (max) : (i))

void Vec3Normalize(vec3_t v)
{
	float invLength = 1.0f / sqrt(DotProduct(v,v));
	Vec3Scale(v, invLength, v);
}

void MapCubeToVec3(vec3_t v, vec2_t st, int face)
{
	int axis = face / 2;
	int neg = face & 1;

	v[0]	= st[0] * 2.0f - 1.0f;
	v[1]	= 1.0f - st[1] * 2.0f;

	v[0]	= (face == 5) ? -v[0] : v[0];
	v[2]	= neg ? v[axis] : -v[axis];
	v[axis] = neg ? -1.0f : 1.0f;
}

// ***************************************************************************

// ***************************************************************************
// Excerpt from https://github.com/castano/nvidia-texture-tools/blob/master/src/nvtt/CubeSurface.cpp

// Copyright (c) 2009-2011 Ignacio Castano <castano@gmail.com>
// 
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:
// 
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

// Solid angle of an axis aligned quad from (0,0,1) to (x,y,1)
// See: http://www.fizzmoll11.com/thesis/ for a derivation of this formula.
static float areaElement(float x, float y) {
    return atan2(x*y, sqrtf(x*x + y*y + 1));
}

// Solid angle of a hemicube texel.
static float solidAngleTerm(unsigned int x, unsigned int y, float inverseEdgeLength) {
    // Transform x,y to [-1, 1] range, offset by 0.5 to point to texel center.
    float u = ((float)(x) + 0.5f) * (2 * inverseEdgeLength) - 1.0f;
    float v = ((float)(y) + 0.5f) * (2 * inverseEdgeLength) - 1.0f;
    //nvDebugCheck(u >= -1.0f && u <= 1.0f);
    //nvDebugCheck(v >= -1.0f && v <= 1.0f);

#if 1
    // Exact solid angle:
    float x0 = u - inverseEdgeLength;
    float y0 = v - inverseEdgeLength;
    float x1 = u + inverseEdgeLength;
    float y1 = v + inverseEdgeLength;
    float solidAngle = areaElement(x0, y0) - areaElement(x0, y1) - areaElement(x1, y0) + areaElement(x1, y1);
    //nvDebugCheck(solidAngle > 0.0f);

    return solidAngle;
#else
    // This formula is equivalent, but not as precise.
    float pixel_area = nv::square(2.0f * inverseEdgeLength);
    float dist_square = 1.0f + nv::square(u) + nv::square(v);
    float cos_theta = 1.0f / sqrt(dist_square);
    float cos_theta_d2 = cos_theta / dist_square; // Funny this is just 1/dist^3 or cos(tetha)^3
    return pixel_area * cos_theta_d2;
#endif
}

// ***************************************************************************

float convertNativeCoordToTexCoord(int coord, int res, float warp)
{
	float tc = (coord + 0.5f) / (float)(res);

	if (warp != 0.0f)
	{
		tc = tc * 2.0f - 1.0f;
		tc *= warp * tc * tc + 1.0f;
		tc = tc * 0.5f + 0.5f;
	}
	
	return tc;
}

float calcWarp(int res)
{
	if (res != 1)
		return res * res / (float)((res - 1) * (res - 1) * (res - 1));
	else
		return 0.0f;
}

void genNorm(float *norm, int x, int y, int face, int res, float warp)
{
	float st[2];

	st[0] = convertNativeCoordToTexCoord(x, res, warp);
	st[1] = convertNativeCoordToTexCoord(y, res, warp);

	MapCubeToVec3(norm, st, face);
	Vec3Normalize(norm);

	norm[3] = solidAngleTerm(x, y, 1.0f / res);
}

float *formatDataForConvolution(uint8_t *rgba8, int inRes)
{
	int face, y, x;
	unsigned char *inPixel = rgba8;
	float *outData = malloc(inRes * inRes * 6 * 7 * sizeof(*outData));
	float *outPixel = outData;

	for (face = 0; face < 6; face++)
	{
		for (y = 0; y < inRes; y++)
		{
			vec2_t st;
			st[1] = convertNativeCoordToTexCoord(y, inRes, 0.0f);

			for (x = 0; x < inRes; x++)
			{
				st[0] = convertNativeCoordToTexCoord(x, inRes, 0.0f);

				MapCubeToVec3(outPixel, st, face);
				Vec3Normalize(outPixel);
				outPixel[3] = solidAngleTerm(x, y, 1.0f / inRes);
				outPixel[4] = ryg_srgb8_to_float(*inPixel++);
				outPixel[5] = ryg_srgb8_to_float(*inPixel++);
				outPixel[6] = ryg_srgb8_to_float(*inPixel++);
				inPixel++;
				outPixel += 7;
			}
		}
	}
	
	return outData;
}

void convolveFaceToVector(float outColor[3], float *outWeightAccum, float *vN_vE, float *inDataFP32, int face, int width, int height, float roughness)
{
	float color[3] = {0.0f, 0.0f, 0.0f}, weightAccum = 0.0f;
	float alpha = roughness * roughness;
	float aa = alpha * alpha;
	
	// constants to speed up ggx calculation in the main loop
	float c1 = 0.5f * aa - 0.5f;
	float c2 = c1 + 1.0f;

	int inY;
	for (inY = 0; inY < height; inY++)
	{
		int offset = (face * width * height) + (inY * width);
		float *vL_inColor = inDataFP32 + offset * 7;
		
		int inX;
		for (inX = 0; inX < width; inX++, vL_inColor += 7)
		{
			float NL = DotProduct(vN_vE, vL_inColor);
			if (NL > 0.0f)
				break;
		}

		for (; inX < width; inX++, vL_inColor += 7)
		{
			float NL = DotProduct(vN_vE, vL_inColor);
			if (NL < 0.0f)
				break;

			// since vN == vE, acos(NH) = 0.5 * acos(NL)
			// so calculate NH * NH from NL
			// cos(t)^2 = cos(2t) * 0.5 + 0.5
			
			//float NHNH = NL * 0.5 + 0.5;
			//float d = NHNH * (aa - 1.0f) + 1.0f;
			float d = NL * c1 + c2;
			float dSpecular = aa / (d * d);

			float weight = vL_inColor[3] * dSpecular * NL;
			
			color[0] += vL_inColor[4] * weight;
			color[1] += vL_inColor[5] * weight;
			color[2] += vL_inColor[6] * weight;
			
			weightAccum += weight;
		}
	}
	
	outColor[0] = color[0];
	outColor[1] = color[1];
	outColor[2] = color[2];
	*outWeightAccum = weightAccum;
}

void convolveCubemapToPixel(uint8_t *outData, int outRes, int outNumMips, int outPixelCount, float *inDataFP32, int width, int height)
{
	int outMipRes;
	uint8_t *outPixel = outData + outPixelCount * 4;
	float color[3] = {0.0f, 0.0f, 0.0f};
	vec3_t vN_vE;
	
	// determine outFace, outY, outX
	outMipRes = outRes;
	int outNumFacePixels = 0;
	while(outMipRes)
	{
		outNumFacePixels += outMipRes * outMipRes;
		outMipRes >>= 1;
	}
	
	int outFace = outPixelCount / outNumFacePixels;
	outPixelCount -= outFace * outNumFacePixels;

	outMipRes = outRes;
	int outMipNum = 0;
	while (outPixelCount >= outMipRes * outMipRes)
	{
		outPixelCount -= outMipRes * outMipRes;
		outMipRes >>= 1;
		outMipNum++;
	}
	
	int outY = outPixelCount / outMipRes;
	outPixelCount -= outY * outMipRes;
	int outX = outPixelCount;
	
	// first mip is min roughness
	// last three mips are roughness 1 (~diffuse)
	// only 4x4 (third last mip) should be used in engine
	float roughness = outMipNum / (float)(outNumMips - 3);
	float minRoughness = 0.5f / (float)(outNumMips - 3);
	roughness = CLAMP(roughness, minRoughness, 1.0f);
	
	float outWarp = calcWarp(outMipRes);

	genNorm(vN_vE, outX, outY, outFace, outMipRes, outWarp);

	int outAxis = outFace / 2;
	float weightAccum = 0.0f;
	int inFace;
	for (inFace = 0; inFace < 6; inFace++)
	{
		int inAxis = inFace / 2;
		float faceColor[3];
		float faceWeightAccum = 0.0f;
		
		if (outAxis == inAxis && outFace != inFace)
			continue;
		
		convolveFaceToVector(faceColor, &faceWeightAccum, vN_vE, inDataFP32, inFace, width, height, roughness);
		Vec3Add(color, color, faceColor);
		weightAccum += faceWeightAccum;
	}

	if (weightAccum)
		weightAccum = 1.0f / weightAccum;

	Vec3Scale(color, weightAccum, color);
	
	outPixel[0] = ryg_float_to_srgb8(color[0]);
	outPixel[1] = ryg_float_to_srgb8(color[1]);
	outPixel[2] = ryg_float_to_srgb8(color[2]);
	outPixel[3] = 255;
}

struct convolveInfo
{
	uint8_t  *outData;
	int outRes;
	int outNumMips;
	float *inDataFP32;
	int inWidth;
	int inHeight;
};

void convolveCubemapToPixelThreaded(void *pArg, struct scheduler *s, sched_uint begin, sched_uint end, sched_uint thread)
{
	struct convolveInfo *info = pArg;

	sched_uint i;
	for (i = begin; i < end; i++)
		convolveCubemapToPixel(info->outData, info->outRes, info->outNumMips, i, info->inDataFP32, info->inWidth, info->inHeight);
}

int main(int argc, char *argv[])
{
	char *inFilename = NULL, *outFilename = NULL;
	unsigned char *inData;
	ddsType_t type;
	ddsFlags_t flags;
	int inWidth, inHeight, inNumMips;
	int numThreads = SCHED_DEFAULT;

	printf("\nGGXCC: GGX cube map convolver for ioQuake3's OpenGL2 renderer\n");
	
	int arg;
	for (arg = 1; arg < argc; arg++)
	{
		if (argv[arg][0] == '-')
		{
			if (strcmp(argv[arg], "-o") == 0 && arg + 1 < argc)
			{
				outFilename = argv[arg + 1];
				arg++;
			}
			else if (strcmp(argv[arg], "-t") == 0 && arg + 1 < argc)
			{
				numThreads = atoi(argv[arg + 1]);
				if (numThreads <= 0)
				{
					printf("Error! Number of threads must be >= 1.\n");
					return 0;
				}
				printf("Using %d threads.\n", numThreads);
				arg++;
			}
		}
		else if (!inFilename)
			inFilename = argv[arg];
	}
	
	if (!inFilename)
	{
		printf("Usage: %s [options] <input.dds> -o <output.dds>\n", argv[0]);
		printf("Available options:\n");
		printf("  -o <output.dds> - Set output filename.  Default is output.dds.\n");
		printf("  -t <threads>    - Set number of threads.  Default is all.\n");
		printf("\nOnly dds, 8-bit RGBA files are accepted as input.\n");
		return 0;
	}
	
	if (!outFilename)
		outFilename = "output.dds";
	
	inData = jrcDdsLoad(inFilename, &type, &flags, &inWidth, &inHeight, &inNumMips);
	
	if (!inData)
	{
		printf("Error loading %s!\n", inFilename);
		return 0;
	}
	
	if (type != DDSTYPE_RGBA)
	{
		printf("Error! Image format must be RGBA32!\n");
		return 0;
	}
	
	if (!(flags | DDSFLAG_CUBEMAP))
	{
		printf("Error! File must contain a cubemap!\n");
		return 0;
	}
	
	if (inWidth != inHeight)
	{
		printf("Error! Texture faces must be square!\n");
		return 0;
	}

	int inRes = inWidth;
	int inNumPixels = inWidth * inHeight * 6;

	int outRes = inRes;
	int outNumPixels = 0;
	int mipRes = outRes;
	int numMips = 0;
	int outNumFacePixels = 0;
	while (mipRes)
	{
		outNumFacePixels += mipRes * mipRes;
		numMips++;
		mipRes >>= 1;
	}
	outNumPixels = outNumFacePixels * 6;
	
	void *sched_memory;
	struct scheduler sched;

	if (numThreads != 1)
	{
		sched_size sched_memory_size;
		scheduler_init(&sched, &sched_memory_size, numThreads, 0);
		sched_memory = calloc(sched_memory_size, 1);
		scheduler_start(&sched, sched_memory);
	}
	
	printf("Reading %d pixels (%dx%dx6, 1 mip) from %s\n", inNumPixels, inWidth, inHeight, inFilename);
	printf("Writing %d pixels(%dx%dx6, %d mips) to %s\n", outNumPixels, outRes, outRes, numMips, outFilename);
	printf("Working...\n");
	
	int64_t startTime = jrcGetTime();
	unsigned char *outData = malloc(outNumPixels * 4);
	float *inDataFP32 = formatDataForConvolution(inData, inRes);

	if (numThreads != 1)
	{
		struct sched_task task;
		struct convolveInfo info;
		info.outData = outData;
		info.outRes = outRes;
		info.outNumMips = numMips;
		info.inDataFP32 = inDataFP32;
		info.inWidth = inWidth;
		info.inHeight = inHeight;

		scheduler_add(&task, &sched, convolveCubemapToPixelThreaded, &info, outNumPixels);
		scheduler_join(&sched, &task);
	}
	else
	{
		int i;
		for (i = 0; i < outNumPixels; i++)
			convolveCubemapToPixel(outData, outRes, numMips, i, inDataFP32, inWidth, inHeight);
	}
	
	printf("Saving...\n");
	
	jrcDdsSave(outFilename, DDSTYPE_RGBA, DDSFLAG_CUBEMAP, outRes, outRes, numMips, outData);

	int64_t endTime = jrcGetTime();
	printf("\n%.3f seconds elapsed.\n", (endTime - startTime) / 1000.0f);


	if (numThreads != 1)
	{
		scheduler_stop(&sched);
		free(sched_memory);
	}
	
	return 0;
}