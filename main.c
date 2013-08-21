#include "xtiffio.h"
#include "geotiff.h"
#include <math.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#define TRUE 1
#define FALSE 0

#define OUT_SAMPLEPERPIXEL 3
#define OUT_BITPERSAMPLE 8

#define OUT_FP_MULTIPLIER 100

// GDAL specific tags
#define TIFFTAG_GDAL_METADATA  42112
#define TIFFTAG_GDAL_NODATA    42113
#define TIFFTAG_RPCCOEFFICIENT 50844

static TIFFExtendProc _ParentExtender = NULL;

static const TIFFFieldInfo xtiffFieldInfo[] = {
    { TIFFTAG_GDAL_METADATA, -1, -1, TIFF_ASCII, FIELD_CUSTOM,
      TRUE, FALSE,  (char *)"GDALMetadata" },
    { TIFFTAG_GDAL_NODATA, -1, -1, TIFF_ASCII, FIELD_CUSTOM,
      TRUE, FALSE,  (char *)"GDALNoDataValue" },
    { TIFFTAG_RPCCOEFFICIENT, -1, -1, TIFF_DOUBLE, FIELD_CUSTOM,
      TRUE, TRUE, (char *)"RPCCoefficient" }
};

static void GDALGTiffTagExtender(TIFF *tif) {
    TIFFMergeFieldInfo( tif, xtiffFieldInfo, sizeof(xtiffFieldInfo) / sizeof(xtiffFieldInfo[0]) );  
          
    if (_ParentExtender) {
        (*_ParentExtender)(tif);
    }
}

static void registerGDALGTiffTagExtender() {
    static bool firstTime = true;
    if (!firstTime) {
        return;
    } else {
        firstTime = false;
    }
  
    _ParentExtender = TIFFSetTagExtender(GDALGTiffTagExtender);
}

uint32 imageWidth;
uint32 imageLength;

uint16 geoPixelScaleSize;
double *geoPixelScale;
uint16 geoTiePointsSize;
double *geoTiePoints;
uint16 geoKeyDirectorySize;
uint16 *geoKeyDirectory;
char *geoAsciiParams;
char *geoGdalNodata;

void writeOutputStrips(char *outFile, char *outImageData, uint32 stripSize, uint32 rowsPerStrip) {

    TIFF *outTif = XTIFFOpen(outFile, "w+");
    if (outTif) {
        TIFFSetField(outTif, TIFFTAG_IMAGEWIDTH, imageWidth);  // set the width of the image
        TIFFSetField(outTif, TIFFTAG_IMAGELENGTH, imageLength);    // set the height of the image
        TIFFSetField(outTif, TIFFTAG_SAMPLESPERPIXEL, OUT_SAMPLEPERPIXEL);   // set number of channels per pixel
        TIFFSetField(outTif, TIFFTAG_BITSPERSAMPLE, OUT_BITPERSAMPLE);    // set the size of the channels
        TIFFSetField(outTif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);    // set the origin of the image.
        TIFFSetField(outTif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField(outTif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
        TIFFSetField(outTif, TIFFTAG_ROWSPERSTRIP, rowsPerStrip);

        TIFFSetField(outTif, TIFFTAG_GEOPIXELSCALE, geoPixelScaleSize, geoPixelScale);
        TIFFSetField(outTif, TIFFTAG_GEOTIEPOINTS, geoTiePointsSize, geoTiePoints);
        TIFFSetField(outTif, TIFFTAG_GEOKEYDIRECTORY, geoKeyDirectorySize, geoKeyDirectory);
        TIFFSetField(outTif, TIFFTAG_GEOASCIIPARAMS, geoAsciiParams);
        // TODO String requires some adjustment
        //TIFFSetField(outTif, TIFFTAG_GDAL_NODATA, geoGdalNodata);

        tsize_t lineBytes = OUT_SAMPLEPERPIXEL * imageWidth;
        unsigned char *buf = (unsigned char *)_TIFFmalloc(lineBytes);
        // We set the strip size of the file to be size of one row of pixels

        // Now writing image to the file one strip at a time
        uint32 row;
        for (row = 0; row < imageLength; row++) {
            _TIFFmemcpy(buf, &outImageData[/*(imageLength - row - 1)*/ row * lineBytes], lineBytes);
            if (TIFFWriteEncodedStrip(outTif, row, buf, lineBytes) < 0) {
            //if (TIFFWriteScanline(outTif, buf, row, lineBytes) < 0) {
                printf("- WRITE BREAK -\n");
                break;
            }
        }
        _TIFFfree(buf);

        XTIFFClose(outTif);
    } else {
        printf("Cannot create %s\n", outFile);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: hmapper <file_in> <file_out>\n");
        return(-1);
    }

    registerGDALGTiffTagExtender();

    char *inFile = argv[1];
    char *outFile = argv[2];

    TIFF *inTif = XTIFFOpen(inFile, "r");
    if (inTif) {
        uint16 bitPerSample;
        uint16 planarConfig;
        uint16 samplePerPixel;
        uint16 sampleFormat;
        uint16 fillOrder;
        uint16 compression;
        uint32 rowsPerStrip;
        uint16 bytePerSample;

        ttile_t tile; 
        ttile_t tilesNumber; 
        uint32 tileWidth;
        uint32 tileLength;      

        tstrip_t strip;
        tstrip_t stripsNumber;
        uint32 *stripByteCounts;
        uint32 stripSize;

        uint32 x, y;
        tdata_t buf;        

        unsigned char *outImageData;
        
        TIFFGetField(inTif, TIFFTAG_IMAGEWIDTH, &imageWidth);
        TIFFGetField(inTif, TIFFTAG_IMAGELENGTH, &imageLength);
        TIFFGetField(inTif, TIFFTAG_BITSPERSAMPLE, &bitPerSample);
        TIFFGetField(inTif, TIFFTAG_PLANARCONFIG, &planarConfig);
        TIFFGetField(inTif, TIFFTAG_SAMPLESPERPIXEL, &samplePerPixel);
        TIFFGetField(inTif, TIFFTAG_SAMPLEFORMAT, &sampleFormat);
        TIFFGetField(inTif, TIFFTAG_FILLORDER, &fillOrder);
        TIFFGetField(inTif, TIFFTAG_COMPRESSION, &compression);
        TIFFGetField(inTif, TIFFTAG_ROWSPERSTRIP, &rowsPerStrip);

        TIFFGetField(inTif, TIFFTAG_GEOPIXELSCALE, &geoPixelScaleSize, &geoPixelScale);
        TIFFGetField(inTif, TIFFTAG_GEOTIEPOINTS, &geoTiePointsSize, &geoTiePoints);
        TIFFGetField(inTif, TIFFTAG_GEOKEYDIRECTORY, &geoKeyDirectorySize, &geoKeyDirectory);
        TIFFGetField(inTif, TIFFTAG_GEOASCIIPARAMS, &geoAsciiParams);
        TIFFGetField(inTif, TIFFTAG_GDAL_NODATA, &geoGdalNodata);

        stripsNumber = TIFFNumberOfStrips(inTif);
        tilesNumber = TIFFNumberOfTiles(inTif);
        bytePerSample = ceil(bitPerSample / 8);

        printf("W: %d, H: %d\n", imageWidth, imageLength);
        printf("BPS: %d (%d), SPP: %d, SF: %d, PC: %d\n", bitPerSample, bytePerSample, samplePerPixel, sampleFormat, planarConfig);
        
        // we are working on grayscale images
        assert(samplePerPixel == 1);
        // other is not implemented
        assert(rowsPerStrip == 1);

        // OUTPUT BUFFER
        //printf("%lu\n\n", imageWidth * imageLength * OUT_SAMPLEPERPIXEL * sizeof(unsigned char));
        // TODO: this should be allocated as small as possible and reused. for now is ok but it is junk
        outImageData = (unsigned char *)_TIFFmalloc(imageWidth * imageLength * OUT_SAMPLEPERPIXEL * sizeof(unsigned char));

        if (stripsNumber > 0) {
            TIFFGetField(inTif, TIFFTAG_STRIPBYTECOUNTS, &stripByteCounts);
            printf("%d\n", stripByteCounts[0]); 

            stripSize = TIFFStripSize(inTif);
            int sampleOffset = stripSize / bytePerSample;
            printf("STRIPPED - COUNT %d - SIZE %d x %d\n", stripsNumber, sampleOffset, stripSize);

            buf = _TIFFmalloc(stripSize);
            for (strip = 0; strip < stripsNumber; strip++) {
                // printf("STRIP %d - SZ: %d - N: %d\n", strip, stripSize, stripsNumber);
                TIFFReadEncodedStrip(inTif, strip, buf, stripSize);

                if (planarConfig == PLANARCONFIG_CONTIG) {
                    if (sampleFormat == SAMPLEFORMAT_UINT && bytePerSample == 4) { // uint32
                        uint32 *uintBuf = (uint32 *)buf;

                        int i = 0;
                        int j = 0;
                        uint32 currentPixelHeight;

                        for (i = 0; i < sampleOffset; i++) {
                            currentPixelHeight = uintBuf[i];

                            int outOffset = (strip * sampleOffset + i) * 3;
                            outImageData[outOffset] = (unsigned char)((currentPixelHeight & 0x00FF0000) >> 16);
                            outImageData[outOffset + 1] = (unsigned char)((currentPixelHeight & 0x0000FF00) >> 8);
                            outImageData[outOffset + 2] = (unsigned char)(currentPixelHeight & 0x000000FF);
                        }
                    } else if (sampleFormat == SAMPLEFORMAT_UINT && bytePerSample == 2) { // uint16
                        uint16 *uintBuf = (uint16 *)buf;
                    } else if (sampleFormat == SAMPLEFORMAT_INT) { // two's complement signed integer data

                    } else if (sampleFormat == SAMPLEFORMAT_IEEEFP && bytePerSample == 4) { // IEEE floating point data
                        float *fpBuf = (float *)buf;
                        int i = 0;
                        int j = 0;
                        uint32 currentPixelHeight;

                        for (i = 0; i < sampleOffset; i++) {

                            currentPixelHeight = (uint32)(fpBuf[i] * OUT_FP_MULTIPLIER);

                            int outOffset = (strip * sampleOffset + i) * 3;
                            outImageData[outOffset] = (unsigned char)((currentPixelHeight & 0x00FF0000) >> 16);
                            outImageData[outOffset + 1] = (unsigned char)((currentPixelHeight & 0x0000FF00) >> 8);
                            outImageData[outOffset + 2] = (unsigned char)(currentPixelHeight & 0x000000FF);
                        }

                    } else if (sampleFormat == SAMPLEFORMAT_IEEEFP && bytePerSample == 8) { // IEEE floating point data
                        double *fpBuf = (double *)buf;
                    } else if (sampleFormat == SAMPLEFORMAT_VOID) { // undefined
                    }
                } else { // PLANARCONFIG_SEPARATE

                }

            }
            _TIFFfree(buf);

            writeOutputStrips(outFile, outImageData, stripSize, rowsPerStrip);

        } else if (tilesNumber > 0) {
            TIFFGetField(inTif, TIFFTAG_TILEWIDTH, &tileWidth);
            TIFFGetField(inTif, TIFFTAG_TILELENGTH, &tileLength);

            printf("TILED - N: %d TW: %d, TH: %d\n", tilesNumber, tileWidth, tileLength);

            buf = _TIFFmalloc(TIFFTileSize(inTif));
            for (y = 0; y < imageLength; y += tileLength) {
                for (x = 0; x < imageWidth; x += tileWidth) {
                    TIFFReadTile(inTif, buf, x, y, 0, 0);
                    // do something
                }
            }
            _TIFFfree(buf);
        }

        GTIF *gtif = GTIFNew(inTif);
        if (gtif) {
            GTIFFree(gtif);
        }

        _TIFFfree(outImageData);

        XTIFFClose(inTif);
    }

    return 0;
}