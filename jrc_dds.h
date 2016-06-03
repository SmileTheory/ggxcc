#ifndef INCLUDE_JRCDDS_H
#define INCLUDE_JRCDDS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	DDSFLAG_CUBEMAP    = 0x01,
	DDSFLAG_SRGB       = 0x02,
	DDSFLAG_SIGNED     = 0x04
}
ddsFlags_t;

typedef enum
{
	DDSTYPE_DXT1,
	DDSTYPE_DXT3,
	DDSTYPE_DXT5,
	DDSTYPE_RGTC1,
	DDSTYPE_RGTC2,
	DDSTYPE_BC6H,
	DDSTYPE_BC7,
	DDSTYPE_RGBA
}
ddsType_t;

int jrcDdsMipSize(int width, int height, int mip, ddsType_t type);
void jrcDdsSave(const char *filename, ddsType_t type, ddsFlags_t flags, int width, int height, int numMips, unsigned char *data);
unsigned char *jrcDdsLoad(const char *filename, ddsType_t *type, ddsFlags_t *flags, int *width, int *height, int *numMips);

#ifdef __cplusplus
}
#endif

#endif
#ifdef JRC_DDS_IMPLEMENTATION
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <assert.h>

typedef unsigned char  ui8_t;
typedef unsigned short ui16_t;
typedef unsigned int   ui32_t;

typedef struct ddsHeader_s
{
	ui32_t headerSize;
	ui32_t flags;
	ui32_t height;
	ui32_t width;
	ui32_t pitchOrFirstMipSize;
	ui32_t volumeDepth;
	ui32_t numMips;
	ui32_t reserved1[11];
	ui32_t always_0x00000020;
	ui32_t pixelFormatFlags;
	ui32_t fourCC;
	ui32_t rgbBitCount;
	ui32_t rBitMask;
	ui32_t gBitMask;
	ui32_t bBitMask;
	ui32_t aBitMask;
	ui32_t caps;
	ui32_t caps2;
	ui32_t caps3;
	ui32_t caps4;
	ui32_t reserved2;
}
ddsHeader_t;

// flags:
#define _DDSFLAGS_REQUIRED     0x001007
#define _DDSFLAGS_PITCH        0x8
#define _DDSFLAGS_MIPMAPCOUNT  0x20000
#define _DDSFLAGS_FIRSTMIPSIZE 0x80000
#define _DDSFLAGS_VOLUMEDEPTH  0x800000

// pixelFormatFlags:
#define DDSPF_ALPHAPIXELS 0x1
#define DDSPF_ALPHA       0x2
#define DDSPF_FOURCC      0x4
#define DDSPF_RGB         0x40
#define DDSPF_YUV         0x200
#define DDSPF_LUMINANCE   0x20000

// caps:
#define DDSCAPS_COMPLEX  0x8
#define DDSCAPS_MIPMAP   0x400000
#define DDSCAPS_REQUIRED 0x1000

// caps2:
#define DDSCAPS2_CUBEMAP 0xFE00
#define DDSCAPS2_VOLUME  0x200000

typedef struct ddsHeaderDxt10_s
{
	ui32_t dxgiFormat;
	ui32_t dimensions;
	ui32_t miscFlags;
	ui32_t arraySize;
	ui32_t miscFlags2;
}
ddsHeaderDxt10_t;

// dxgiFormat
// from http://msdn.microsoft.com/en-us/library/windows/desktop/bb173059%28v=vs.85%29.aspx
typedef enum DXGI_FORMAT { 
  DXGI_FORMAT_UNKNOWN                     = 0,
  DXGI_FORMAT_R32G32B32A32_TYPELESS       = 1,
  DXGI_FORMAT_R32G32B32A32_FLOAT          = 2,
  DXGI_FORMAT_R32G32B32A32_UINT           = 3,
  DXGI_FORMAT_R32G32B32A32_SINT           = 4,
  DXGI_FORMAT_R32G32B32_TYPELESS          = 5,
  DXGI_FORMAT_R32G32B32_FLOAT             = 6,
  DXGI_FORMAT_R32G32B32_UINT              = 7,
  DXGI_FORMAT_R32G32B32_SINT              = 8,
  DXGI_FORMAT_R16G16B16A16_TYPELESS       = 9,
  DXGI_FORMAT_R16G16B16A16_FLOAT          = 10,
  DXGI_FORMAT_R16G16B16A16_UNORM          = 11,
  DXGI_FORMAT_R16G16B16A16_UINT           = 12,
  DXGI_FORMAT_R16G16B16A16_SNORM          = 13,
  DXGI_FORMAT_R16G16B16A16_SINT           = 14,
  DXGI_FORMAT_R32G32_TYPELESS             = 15,
  DXGI_FORMAT_R32G32_FLOAT                = 16,
  DXGI_FORMAT_R32G32_UINT                 = 17,
  DXGI_FORMAT_R32G32_SINT                 = 18,
  DXGI_FORMAT_R32G8X24_TYPELESS           = 19,
  DXGI_FORMAT_D32_FLOAT_S8X24_UINT        = 20,
  DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS    = 21,
  DXGI_FORMAT_X32_TYPELESS_G8X24_UINT     = 22,
  DXGI_FORMAT_R10G10B10A2_TYPELESS        = 23,
  DXGI_FORMAT_R10G10B10A2_UNORM           = 24,
  DXGI_FORMAT_R10G10B10A2_UINT            = 25,
  DXGI_FORMAT_R11G11B10_FLOAT             = 26,
  DXGI_FORMAT_R8G8B8A8_TYPELESS           = 27,
  DXGI_FORMAT_R8G8B8A8_UNORM              = 28,
  DXGI_FORMAT_R8G8B8A8_UNORM_SRGB         = 29,
  DXGI_FORMAT_R8G8B8A8_UINT               = 30,
  DXGI_FORMAT_R8G8B8A8_SNORM              = 31,
  DXGI_FORMAT_R8G8B8A8_SINT               = 32,
  DXGI_FORMAT_R16G16_TYPELESS             = 33,
  DXGI_FORMAT_R16G16_FLOAT                = 34,
  DXGI_FORMAT_R16G16_UNORM                = 35,
  DXGI_FORMAT_R16G16_UINT                 = 36,
  DXGI_FORMAT_R16G16_SNORM                = 37,
  DXGI_FORMAT_R16G16_SINT                 = 38,
  DXGI_FORMAT_R32_TYPELESS                = 39,
  DXGI_FORMAT_D32_FLOAT                   = 40,
  DXGI_FORMAT_R32_FLOAT                   = 41,
  DXGI_FORMAT_R32_UINT                    = 42,
  DXGI_FORMAT_R32_SINT                    = 43,
  DXGI_FORMAT_R24G8_TYPELESS              = 44,
  DXGI_FORMAT_D24_UNORM_S8_UINT           = 45,
  DXGI_FORMAT_R24_UNORM_X8_TYPELESS       = 46,
  DXGI_FORMAT_X24_TYPELESS_G8_UINT        = 47,
  DXGI_FORMAT_R8G8_TYPELESS               = 48,
  DXGI_FORMAT_R8G8_UNORM                  = 49,
  DXGI_FORMAT_R8G8_UINT                   = 50,
  DXGI_FORMAT_R8G8_SNORM                  = 51,
  DXGI_FORMAT_R8G8_SINT                   = 52,
  DXGI_FORMAT_R16_TYPELESS                = 53,
  DXGI_FORMAT_R16_FLOAT                   = 54,
  DXGI_FORMAT_D16_UNORM                   = 55,
  DXGI_FORMAT_R16_UNORM                   = 56,
  DXGI_FORMAT_R16_UINT                    = 57,
  DXGI_FORMAT_R16_SNORM                   = 58,
  DXGI_FORMAT_R16_SINT                    = 59,
  DXGI_FORMAT_R8_TYPELESS                 = 60,
  DXGI_FORMAT_R8_UNORM                    = 61,
  DXGI_FORMAT_R8_UINT                     = 62,
  DXGI_FORMAT_R8_SNORM                    = 63,
  DXGI_FORMAT_R8_SINT                     = 64,
  DXGI_FORMAT_A8_UNORM                    = 65,
  DXGI_FORMAT_R1_UNORM                    = 66,
  DXGI_FORMAT_R9G9B9E5_SHAREDEXP          = 67,
  DXGI_FORMAT_R8G8_B8G8_UNORM             = 68,
  DXGI_FORMAT_G8R8_G8B8_UNORM             = 69,
  DXGI_FORMAT_BC1_TYPELESS                = 70,
  DXGI_FORMAT_BC1_UNORM                   = 71,
  DXGI_FORMAT_BC1_UNORM_SRGB              = 72,
  DXGI_FORMAT_BC2_TYPELESS                = 73,
  DXGI_FORMAT_BC2_UNORM                   = 74,
  DXGI_FORMAT_BC2_UNORM_SRGB              = 75,
  DXGI_FORMAT_BC3_TYPELESS                = 76,
  DXGI_FORMAT_BC3_UNORM                   = 77,
  DXGI_FORMAT_BC3_UNORM_SRGB              = 78,
  DXGI_FORMAT_BC4_TYPELESS                = 79,
  DXGI_FORMAT_BC4_UNORM                   = 80,
  DXGI_FORMAT_BC4_SNORM                   = 81,
  DXGI_FORMAT_BC5_TYPELESS                = 82,
  DXGI_FORMAT_BC5_UNORM                   = 83,
  DXGI_FORMAT_BC5_SNORM                   = 84,
  DXGI_FORMAT_B5G6R5_UNORM                = 85,
  DXGI_FORMAT_B5G5R5A1_UNORM              = 86,
  DXGI_FORMAT_B8G8R8A8_UNORM              = 87,
  DXGI_FORMAT_B8G8R8X8_UNORM              = 88,
  DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM  = 89,
  DXGI_FORMAT_B8G8R8A8_TYPELESS           = 90,
  DXGI_FORMAT_B8G8R8A8_UNORM_SRGB         = 91,
  DXGI_FORMAT_B8G8R8X8_TYPELESS           = 92,
  DXGI_FORMAT_B8G8R8X8_UNORM_SRGB         = 93,
  DXGI_FORMAT_BC6H_TYPELESS               = 94,
  DXGI_FORMAT_BC6H_UF16                   = 95,
  DXGI_FORMAT_BC6H_SF16                   = 96,
  DXGI_FORMAT_BC7_TYPELESS                = 97,
  DXGI_FORMAT_BC7_UNORM                   = 98,
  DXGI_FORMAT_BC7_UNORM_SRGB              = 99,
  DXGI_FORMAT_AYUV                        = 100,
  DXGI_FORMAT_Y410                        = 101,
  DXGI_FORMAT_Y416                        = 102,
  DXGI_FORMAT_NV12                        = 103,
  DXGI_FORMAT_P010                        = 104,
  DXGI_FORMAT_P016                        = 105,
  DXGI_FORMAT_420_OPAQUE                  = 106,
  DXGI_FORMAT_YUY2                        = 107,
  DXGI_FORMAT_Y210                        = 108,
  DXGI_FORMAT_Y216                        = 109,
  DXGI_FORMAT_NV11                        = 110,
  DXGI_FORMAT_AI44                        = 111,
  DXGI_FORMAT_IA44                        = 112,
  DXGI_FORMAT_P8                          = 113,
  DXGI_FORMAT_A8P8                        = 114,
  DXGI_FORMAT_B4G4R4A4_UNORM              = 115,
  DXGI_FORMAT_FORCE_UINT                  = 0xffffffffUL
} DXGI_FORMAT;

#define EncodeFourCC(x) ((((ui32_t)((x)[0]))      ) | \
                           (((ui32_t)((x)[1])) << 8 ) | \
                           (((ui32_t)((x)[2])) << 16) | \
                           (((ui32_t)((x)[3])) << 24) )

int jrcDdsMipSize(int width, int height, int mip, ddsType_t type)
{
	int size;

	width >>= mip;
	height >>= mip;

	if (width == 0 && height == 0) return 0;

	if (width == 0) width = 1;
	if (height == 0) height = 1;

	if (type == DDSTYPE_RGBA) return width * height * 4;

	size = ((width + 3) / 4) * ((height + 3) / 4);

	switch(type)
	{
		case DDSTYPE_DXT1:
		case DDSTYPE_RGTC1:
			return size * 8;
			break;

		case DDSTYPE_DXT3:
		case DDSTYPE_DXT5:
		case DDSTYPE_RGTC2:
		case DDSTYPE_BC6H:
		case DDSTYPE_BC7:
			return size * 16;
			break;

		default:
			break;
	}

	return width * height * 4;
}


static int MakeDdsHeader(ddsHeader_t *ddsHeader, ddsHeaderDxt10_t *ddsHeaderDxt10, ddsType_t type, ddsFlags_t flags, int width, int height, int numMips)
{
	int hasDxt10Header = 0;

	memset(ddsHeader, 0, sizeof(*ddsHeader));
	ddsHeader->headerSize = 0x0000007c;
	ddsHeader->flags = _DDSFLAGS_REQUIRED;
	ddsHeader->height = height;
	ddsHeader->width = width;
	ddsHeader->always_0x00000020 = 0x00000020;
	ddsHeader->caps = DDSCAPS_COMPLEX | DDSCAPS_REQUIRED;

	if (flags & DDSFLAG_CUBEMAP)
		ddsHeader->caps2 = DDSCAPS2_CUBEMAP;

	if (numMips > 1)
	{
		ddsHeader->flags |= _DDSFLAGS_MIPMAPCOUNT;
		ddsHeader->numMips = numMips;
		ddsHeader->caps |= DDSCAPS_MIPMAP;
	}

	memset(ddsHeaderDxt10, 0, sizeof(*ddsHeaderDxt10));
	ddsHeaderDxt10->dimensions = 3;

	if (flags & DDSFLAG_CUBEMAP)
		ddsHeaderDxt10->miscFlags = 0x4;

	ddsHeaderDxt10->arraySize = 1;
	
	if (flags & DDSFLAG_SRGB)
	{
		hasDxt10Header = 1;

		switch (type)
		{
			case DDSTYPE_DXT1:
				ddsHeaderDxt10->dxgiFormat = DXGI_FORMAT_BC1_UNORM_SRGB;
				break;

			case DDSTYPE_DXT3:
				ddsHeaderDxt10->dxgiFormat = DXGI_FORMAT_BC2_UNORM_SRGB;
				break;

			case DDSTYPE_DXT5:
				ddsHeaderDxt10->dxgiFormat = DXGI_FORMAT_BC3_UNORM_SRGB;
				break;

			case DDSTYPE_BC7:
				ddsHeaderDxt10->dxgiFormat = DXGI_FORMAT_BC7_UNORM_SRGB;
				break;

			case DDSTYPE_RGBA:
				ddsHeaderDxt10->dxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
				break;

			default:
				// not supported
				break;
		}
	}
	else if (flags & DDSFLAG_SIGNED)
	{
		switch(type)
		{
			case DDSTYPE_RGTC2:
				ddsHeader->pixelFormatFlags = DDSPF_FOURCC;
				ddsHeader->fourCC = EncodeFourCC("BC4S");
				break;

			case DDSTYPE_RGTC1:
				ddsHeader->pixelFormatFlags = DDSPF_FOURCC;
				ddsHeader->fourCC = EncodeFourCC("BC5S");
				break;

			case DDSTYPE_BC6H:
				hasDxt10Header = 1;
				ddsHeaderDxt10->dxgiFormat = DXGI_FORMAT_BC6H_SF16;
				break;

			case DDSTYPE_RGBA:
				hasDxt10Header = 1;
				ddsHeaderDxt10->dxgiFormat = DXGI_FORMAT_R8G8B8A8_SNORM;
				break;

			default:
				// not supported
				break;
		}
	}
	else
	{
		switch(type)
		{
			case DDSTYPE_DXT1:
				ddsHeader->pixelFormatFlags = DDSPF_FOURCC;
				ddsHeader->fourCC = EncodeFourCC("DXT1");
				break;

			case DDSTYPE_DXT3:
				ddsHeader->pixelFormatFlags = DDSPF_FOURCC;
				ddsHeader->fourCC = EncodeFourCC("DXT3");
				break;

			case DDSTYPE_DXT5:
				ddsHeader->pixelFormatFlags = DDSPF_FOURCC;
				ddsHeader->fourCC = EncodeFourCC("DXT5");
				break;

			case DDSTYPE_RGTC2:
				ddsHeader->pixelFormatFlags = DDSPF_FOURCC;
				ddsHeader->fourCC = EncodeFourCC("ATI1");
				break;

			case DDSTYPE_RGTC1:
				ddsHeader->pixelFormatFlags = DDSPF_FOURCC;
				ddsHeader->fourCC = EncodeFourCC("ATI2");
				break;

			case DDSTYPE_BC6H:
				hasDxt10Header = 1;
				ddsHeaderDxt10->dxgiFormat = DXGI_FORMAT_BC6H_UF16;
				break;

			case DDSTYPE_BC7:
				hasDxt10Header = 1;
				ddsHeaderDxt10->dxgiFormat = DXGI_FORMAT_BC7_UNORM;
				break;

			case DDSTYPE_RGBA:
				ddsHeader->pixelFormatFlags = DDSPF_RGB | DDSPF_ALPHAPIXELS;
				ddsHeader->rgbBitCount = 32;
				ddsHeader->rBitMask = 0x000000ff;
				ddsHeader->gBitMask = 0x0000ff00;
				ddsHeader->bBitMask = 0x00ff0000;
				ddsHeader->aBitMask = 0xff000000;
				break;

			default:
				// not supported
				break;
		}
	}

	if (hasDxt10Header)
	{
		ddsHeader->pixelFormatFlags = DDSPF_FOURCC;
		ddsHeader->fourCC = EncodeFourCC("DX10");
		
		// hack for pico pixel
		if (ddsHeaderDxt10->dxgiFormat == DXGI_FORMAT_BC6H_UF16)
			ddsHeaderDxt10->dxgiFormat = DXGI_FORMAT_BC6H_SF16;
		else if (ddsHeaderDxt10->dxgiFormat == DXGI_FORMAT_BC6H_SF16)
			ddsHeaderDxt10->dxgiFormat = DXGI_FORMAT_BC6H_UF16;
	}

	return hasDxt10Header;
}

void jrcDdsSave(const char *filename, ddsType_t type, ddsFlags_t flags, int width, int height, int numMips, ui8_t *data)
{
	ddsHeader_t ddsHeader;
	ddsHeaderDxt10_t ddsHeaderDxt10;
	int hasDxt10Header;
	FILE *fp;
	int mipSizes[32];
	int totalSize;
	int x, y, i;

	assert(numMips < 32);

	totalSize = 0;
	x = width;
	y = height;
	for (i = 0; i < numMips; i++)
	{
		totalSize += mipSizes[i] = jrcDdsMipSize(width, height, i, type);

		x >>= 1;
		y >>= 1;

		if (x == 0 && y == 0)
		{
			numMips = i + 1;
			break;
		}

		if (x == 0) x = 1;
		if (y == 0) y = 1;
	}

	if (flags & DDSFLAG_CUBEMAP)
		totalSize *= 6;

	hasDxt10Header = MakeDdsHeader(&ddsHeader, &ddsHeaderDxt10, type, flags, width, height, numMips);
	
	fp = fopen(filename, "wb");
	fwrite("DDS ", 4, 1, fp);
	fwrite(&ddsHeader, sizeof(ddsHeader), 1, fp);
	if (hasDxt10Header)
		fwrite(&ddsHeaderDxt10, sizeof(ddsHeaderDxt10), 1, fp);
	fwrite(data, totalSize, 1, fp);
	fclose(fp);
}

ui8_t *jrcDdsLoad(const char *filename, ddsType_t *type, ddsFlags_t *flags, int *width, int *height, int *numMips)
{
	ddsHeader_t ddsHeader;
	ddsHeaderDxt10_t ddsHeaderDxt10;
	ui32_t id;
	ui8_t *data;
	FILE *fp;
	int fileSize, hasDxt10Header;

	fp = fopen(filename, "rb");
	if (!fp)
		return NULL;

	fseek(fp, 0, SEEK_END);
	fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	
	if (fileSize < 4 + sizeof(ddsHeader))
	{
		fclose(fp);
		return NULL;
	}

	fread(&id, sizeof(id), 1, fp);
	fileSize -= 4;
	if (id != EncodeFourCC("DDS "))
	{
		fclose(fp);
		return NULL;
	}

	fread(&ddsHeader, sizeof(ddsHeader), 1, fp);
	fileSize -= sizeof(ddsHeader);

	hasDxt10Header = 0;
	if ((ddsHeader.pixelFormatFlags & DDSPF_FOURCC) && ddsHeader.fourCC == EncodeFourCC("DX10"))
	{
		if (fileSize < sizeof(ddsHeaderDxt10))
		{
			fclose(fp);
			return NULL;
		}

		fread(&ddsHeaderDxt10, sizeof(ddsHeaderDxt10), 1, fp);
		fileSize -= sizeof(ddsHeaderDxt10);
		hasDxt10Header = 1;
	}

	*width = ddsHeader.width;
	*height = ddsHeader.height;

	if (ddsHeader.flags & _DDSFLAGS_MIPMAPCOUNT)
		*numMips = ddsHeader.numMips;
	else
		*numMips = 1;

	*flags = 0;

	if ((ddsHeader.caps2 & DDSCAPS2_CUBEMAP) == DDSCAPS2_CUBEMAP)
		*flags |= DDSFLAG_CUBEMAP;

	if (hasDxt10Header)
	{
		// FIXME: support miscFlags, arraySize, miscFlags2, and other dxgi formats
		switch(ddsHeaderDxt10.dxgiFormat)
		{
			case DXGI_FORMAT_BC1_TYPELESS:
			case DXGI_FORMAT_BC1_UNORM:
			case DXGI_FORMAT_BC1_UNORM_SRGB:
				*type = DDSTYPE_DXT1;
				break;

			case DXGI_FORMAT_BC2_TYPELESS:
			case DXGI_FORMAT_BC2_UNORM:
			case DXGI_FORMAT_BC2_UNORM_SRGB:
				*type = DDSTYPE_DXT3;
				break;

			case DXGI_FORMAT_BC3_TYPELESS:
			case DXGI_FORMAT_BC3_UNORM:
			case DXGI_FORMAT_BC3_UNORM_SRGB:
				*type = DDSTYPE_DXT5;
				break;

			case DXGI_FORMAT_BC4_TYPELESS:
			case DXGI_FORMAT_BC4_UNORM:
			case DXGI_FORMAT_BC4_SNORM:
				*type = DDSTYPE_RGTC2;
				break;

			case DXGI_FORMAT_BC5_TYPELESS:
			case DXGI_FORMAT_BC5_UNORM:
			case DXGI_FORMAT_BC5_SNORM:
				*type = DDSTYPE_RGTC1;
				break;

			case DXGI_FORMAT_BC6H_TYPELESS:
			case DXGI_FORMAT_BC6H_UF16:
			case DXGI_FORMAT_BC6H_SF16:
				*type = DDSTYPE_BC6H;
				break;

			case DXGI_FORMAT_BC7_TYPELESS:
			case DXGI_FORMAT_BC7_UNORM:
			case DXGI_FORMAT_BC7_UNORM_SRGB:
				*type = DDSTYPE_BC7;
				break;

			case DXGI_FORMAT_R8G8B8A8_UNORM:
			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
			case DXGI_FORMAT_R8G8B8A8_SNORM:
				*type = DDSTYPE_RGBA;
				break;

			default:
				fclose(fp);
				return NULL;
				break;
		}

		switch(ddsHeaderDxt10.dxgiFormat)
		{
			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
			case DXGI_FORMAT_BC1_UNORM_SRGB:
			case DXGI_FORMAT_BC2_UNORM_SRGB:
			case DXGI_FORMAT_BC3_UNORM_SRGB:
			case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
			case DXGI_FORMAT_BC7_UNORM_SRGB:
				*flags |= DDSFLAG_SRGB;
				break;

			case DXGI_FORMAT_R16G16B16A16_SNORM:
			case DXGI_FORMAT_R8G8B8A8_SNORM:
			case DXGI_FORMAT_R16G16_SNORM:
			case DXGI_FORMAT_R8G8_SNORM:
			case DXGI_FORMAT_R16_SNORM:
			case DXGI_FORMAT_R8_SNORM:
			case DXGI_FORMAT_BC4_SNORM:
			case DXGI_FORMAT_BC5_SNORM:
			case DXGI_FORMAT_BC6H_SF16:
				*flags |= DDSFLAG_SIGNED;
				break;

			default:
				break;
		}

	}
	else
	{
		if (ddsHeader.pixelFormatFlags & DDSPF_FOURCC)
		{
			if (ddsHeader.fourCC == EncodeFourCC("DXT1"))
				*type = DDSTYPE_DXT1;
			else if (ddsHeader.fourCC == EncodeFourCC("DXT2"))
				*type = DDSTYPE_DXT3;
			else if (ddsHeader.fourCC == EncodeFourCC("DXT3"))
				*type = DDSTYPE_DXT3;
			else if (ddsHeader.fourCC == EncodeFourCC("DXT4"))
				*type = DDSTYPE_DXT5;
			else if (ddsHeader.fourCC == EncodeFourCC("DXT5"))
				*type = DDSTYPE_DXT5;
			else if (ddsHeader.fourCC == EncodeFourCC("ATI1"))
				*type = DDSTYPE_RGTC2;
			else if (ddsHeader.fourCC == EncodeFourCC("BC4U"))
				*type = DDSTYPE_RGTC2;
			else if (ddsHeader.fourCC == EncodeFourCC("BC4S"))
			{
				*type = DDSTYPE_RGTC2;
				*flags |= DDSFLAG_SIGNED;
			}
			else if (ddsHeader.fourCC == EncodeFourCC("ATI2"))
				*type = DDSTYPE_RGTC1;
			else if (ddsHeader.fourCC == EncodeFourCC("BC5U"))
				*type = DDSTYPE_RGTC1;
			else if (ddsHeader.fourCC == EncodeFourCC("BC5S"))
			{
				*type = DDSTYPE_RGTC1;
				*flags |= DDSFLAG_SIGNED;
			}
			else
			{
				// not supported
				fclose(fp);
				return NULL;
			}
		}
		else if (ddsHeader.pixelFormatFlags == (DDSPF_RGB | DDSPF_ALPHAPIXELS)
			&& ddsHeader.rgbBitCount == 32 
			&& ddsHeader.rBitMask == 0x000000ff
			&& ddsHeader.gBitMask == 0x0000ff00
			&& ddsHeader.bBitMask == 0x00ff0000
			&& ddsHeader.aBitMask == 0xff000000)
		{
			*type = DDSTYPE_RGBA;
		}
		else
		{
			// not supported
			fclose(fp);
			return NULL;
		}
	}

	data = malloc(fileSize);
	fread(data, fileSize, 1, fp);
	fclose(fp);

	return data;
}
#endif
