#include "image_debug.h"
#include "png.h"
#include "../zlib/lz.h"
#include <string.h>
#include <stdlib.h>

#define DEFAULT_ALLOCATION_SIZE 65535

#define bswap32(x) \
	(((x >> 24) & 0x000000FF) | \
	 ((x >> 8) & 0x0000FF00)  | \
	 ((x << 8) & 0x00FF0000)  | \
	 ((x << 24) & 0xFF000000))

#define Skip(png, n) png->reader += n;

// These are static APIs, they don't require debug level NULL checking.
static inline uint8_t GetUInt8(PNG* png) {
	return *png->reader++;
}

static inline uint16_t GetUInt16be(PNG* png) {
	uint16_t b = GetUInt8(png);
	return (b << 8) | GetUInt8(png);
}

static inline uint32_t GetUInt32be(PNG* png) {
	uint32_t b = GetUInt16be(png);
	return (b << 16) | GetUInt16be(png);
}

static inline uint16_t GetUInt16(PNG* png) {
	uint16_t ret = *(uint16_t*)png->reader;
	png->reader += 2;
	return ret;
}

static inline uint32_t GetUInt32(PNG* png) {
	uint32_t ret = *(uint32_t*)png->reader;
	png->reader += 4;
	return ret;
}

static inline const uint8_t* GetBytes(PNG* png, uint64_t n) {
	const uint8_t* ret = png->reader;
	png->reader += n;
	return ret;
}

inline uint32_t BytesPerColorTypePNG(uint32_t type) {
	return (type == 2 || type == 3) ? 3 : (type == 6) ? 4 : (type == 0) ? 1 : 0;
}

static int32_t PaethPredictor(int32_t a, int32_t b, int32_t c) {
	int32_t p = a + b - c;
	int32_t pa = abs((int32_t)(p - a));
	int32_t pb = abs((int32_t)(p - b));
	int32_t pc = abs((int32_t)(p - c));
	if (pa <= pb && pa <= pc) return a;
	else if (pb <= pc) return b;
	else return c;
}

static void DefilterPNG(PNG* png, uint8_t* unfiltered, uint8_t* pixels) {
	uint32_t outIdx = 0;
	uint32_t bpp = png->isPlte ? 1 : BytesPerColorTypePNG(png->ihdr.colorType);
	uint32_t stride = png->ihdr.imageWidth * bpp;
	uint32_t filterType = 0;
	uint8_t* offset = unfiltered;
	uint8_t* prevRow = NULL;

	for (uint32_t i = 0; i < png->ihdr.imageHeight; ++i) {
		filterType = *offset++;

		switch (filterType) {
		case 0: // None: Raw(x)
		{
			for (uint32_t j = 0; j < stride; ++j) {
				pixels[outIdx++] = offset[j];
			}
			break;
		}
		case 1: // Sub: Sub(x) + Raw(x-bpp)
		{
			for (uint32_t j = 0; j < stride; ++j) {
				uint8_t left = (j >= bpp) ? pixels[outIdx - bpp] : 0;
				pixels[outIdx++] = offset[j] + left;
			}
			break;
		}

		case 2: // Up: Up(x) + Prior(x)
		{
			for (uint32_t j = 0; j < stride; ++j) {
				pixels[outIdx++] = offset[j] + (i ? prevRow[j] : 0);
			}
			break;
		}
		case 3: // Average: Average(x) + floor((Raw(x-bpp)+Prior(x))/2)
		{
			for (uint32_t j = 0; j < stride; ++j) {
				uint8_t left = (j >= bpp) ? pixels[outIdx - bpp] : 0;
				uint8_t up = prevRow ? prevRow[j] : 0;

				pixels[outIdx++] = offset[j] + ((left + up) >> 1);
			}
			break;
		}
		case 4: // Paeth: Paeth(x) + PaethPredictor(Raw(x-bpp), Prior(x), Prior(x-bpp))
		{
			for (uint32_t j = 0; j < stride; ++j) {
				uint8_t left = (j >= bpp) ? pixels[outIdx - bpp] : 0;
				uint8_t up = prevRow ? prevRow[j] : 0;
				uint8_t upLeft = (j >= bpp && prevRow) ? prevRow[j - bpp] : 0;
				pixels[outIdx++] = offset[j] + PaethPredictor(left, up, upLeft);
			}
			break;
		}
		default:
		{
			break;
		}
		}
		prevRow = pixels + i * stride;
		offset += stride;
	}
}

static void DeindexPNG(PNG* png, uint8_t* indices, uint8_t* pixels) {
	size_t size = (uint64_t)png->ihdr.imageWidth * (uint64_t)png->ihdr.imageHeight;
	for (size_t i = 0; i < size; ++i) {
		*(RGB*)&pixels[3 * i] = png->plte.pEntries[indices[i]];
	}
}

int32_t IsPNG(const uint8_t* data, uint64_t size) {
	DEBUG(data);

	static const uint8_t PNG_SIG[] = { 137, 80, 78, 71, 13, 10, 26, 10 };
	if (size < 67 || *(uint64_t*)data != *(uint64_t*)PNG_SIG) {
		return 0;
	}
	return 1;
}

int32_t InitPNG(PNG* png, const uint8_t* data, uint64_t inSize)
{
	DEBUG(png);
	DEBUG(data);
	DEBUG(inSize);

	if (!IsPNG(data, inSize)) {
		return -3;
	}
	png->isInit = 1;
	png->reader = data;
	png->chunksSize = 0;
	png->chunksCapacity = 20;

	Skip(png, 8);

	png->pChunks = (PngChunk*)calloc(png->chunksCapacity, sizeof(PngChunk));
	if (!png->pChunks) {
		return -2;
	}

	PngChunk chunk = { 0 };
	do {
		chunk.length = GetUInt32be(png);
		chunk.u32type = GetUInt32(png);
		chunk.data = GetBytes(png, chunk.length);
		chunk.crc = GetUInt32be(png);

		switch (chunk.u32type) {
		case 'RDHI':
		{
			png->ihdr = *(IHDR*)chunk.data;
			png->ihdr.imageWidth = bswap32(png->ihdr.imageWidth);
			png->ihdr.imageHeight = bswap32(png->ihdr.imageHeight);
			break;
		}
		case 'ETLP':
		{
			png->isPlte = 1;
			if (chunk.length % 3 != 0) {
				free(png->pChunks);
				return -3;
			}
			png->plte.nEntries = chunk.length / 3;
			png->plte.pEntries = (const RGB*)chunk.data;
			break;
		}
		case 'AMAg':
		{
			png->gama = bswap32(*(uint32_t*)chunk.data);
			break;
		}
		default:
		{
			break;
		}
		}

		if (png->chunksCapacity <= png->chunksSize) {
			png->chunksCapacity *= 2;
			PngChunk* tmp = (PngChunk*)realloc(png->pChunks, png->chunksCapacity * sizeof(PngChunk));
			if (!tmp) {
				free(png->pChunks);
				return -2;
			}
			png->pChunks = tmp;
		}
		png->pChunks[png->chunksSize++] = chunk;

	} while (chunk.u32type != 'DNEI');

	return 1;
}

int32_t LoadPNG(PNG* png, uint8_t* output, uint64_t outSize)
{
	DEBUG(png);
	DEBUG(output);
	DEBUG(outSize);

	if (!png->isInit) {
		return -6;
	}

	int32_t error = 0;
	ZlibReader zlib = { 0 };
	uint8_t *uncompressed = NULL;

	uint64_t compressedCapacity = DEFAULT_ALLOCATION_SIZE;
	uint64_t compressedSize = 0;
	uint8_t* compressed = calloc(1, compressedCapacity);

	if (!compressed) {
		return -2;
	}

	for (uint32_t i = 0; i < png->chunksSize; ++i) {
		if (png->pChunks[i].u32type == 'TADI') {
			uint64_t newSize = compressedSize + png->pChunks[i].length;
			if (compressedCapacity <= newSize) {
				compressedCapacity *= 8;
				uint8_t* tmp = realloc(compressed, compressedCapacity);
				if (!tmp) {
					free(compressed);
					return -2;
				}
				compressed = tmp;
			}
			memcpy(compressed + compressedSize, png->pChunks[i].data, png->pChunks[i].length);
			compressedSize = newSize;
		}
	}

	uncompressed = (uint8_t*)calloc(1, outSize * 2);
	if (!uncompressed) {
		free(compressed);
		return -2;
	}

	error = lzInflateInit(&zlib, compressed, compressedSize);
	if (error < 0) {
		free(compressed);
		free(uncompressed);
		return error;
	}
	error = lzInflate(&zlib, uncompressed, outSize * 2);
	if (error < 0) {
		free(compressed);
		free(uncompressed);
		return error;
	}

	if (png->isPlte) {
		uint8_t* tmp = (uint8_t*)calloc(1, outSize);
		if (!tmp) {
			free(uncompressed);
			free(compressed);
			return -2;
		}
		DefilterPNG(png, uncompressed, tmp);
		DeindexPNG(png, tmp, output);
		free(tmp);
	}
	else {
		DefilterPNG(png, uncompressed, output);
	}

	free(uncompressed);
	free(compressed);

	return 1;
}

void FreePNG(PNG* png) {
	if (!png) return;
	if (!png->pChunks) return;
	free(png->pChunks);
	png->chunksSize = 0;
	png->chunksCapacity = 0;
}