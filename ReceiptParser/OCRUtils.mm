//
//  OCRUtils.m
//  Windfall
//
//  Created by Patrick Questembert on 4/1/15.
//  Copyright (c) 2015 Windfall. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIImage.h>
#import <UIKit/UIImagePickerController.h>

#import "common.hh"
#import "OCRUtils.h"
#import "OCRResults.h"
#import "Image.h"
#import "receiptParser.hh"

void *getOBJCMatches(void *ptr) {
	return ((__bridge void *)[OCRGlobalResults buildMatchesFromMatchVector:ptr ToMatches:nil]);
}

bool OCRUtilsOverlappingX(CGRect r1, CGRect r2) {
	float MinX = MAX(r1.origin.x, r2.origin.x);
	float MaxX = MIN(r1.origin.x+r1.size.width-1, r2.origin.x+r2.size.width-1);
	if (MaxX < MinX)
		return false;
	else
		return true;
}

void saveRawImageToDisk(uint8_t *imageData, size_t width, size_t height) {
	CGContextRef gscr = CGBitmapContextCreate(imageData, width, height, 8, width, CGColorSpaceCreateDeviceGray(), kCGImageAlphaNone);
	CGImageRef gsir = CGBitmapContextCreateImage(gscr);
	
	UIImage *toSave = [[UIImage alloc] initWithCGImage:gsir];
	CGImageRelease(gsir);
	CGContextRelease(gscr);
	
	UIImageWriteToSavedPhotosAlbum(toSave, nil, NULL, nil);
}

CGContextRef OCRUtilsCreateARGBBitmapContextWithData (size_t pixelsWide, size_t pixelsHigh) //leaky
{
	CGContextRef    context = NULL;
	CGColorSpaceRef colorSpace;
	void *          bitmapData;
	NSInteger       bitmapByteCount;
	NSInteger       bitmapBytesPerRow;
	
	// Declare the number of bytes per row. Each pixel in the bitmap in this
	// example is represented by 4 bytes; 8 bits each of red, green, blue, and
	// alpha.
	bitmapBytesPerRow   = (pixelsWide * 4);
	bitmapByteCount     = (bitmapBytesPerRow * pixelsHigh);
	
	// Use the generic RGB color space.
	//colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
	colorSpace = CGColorSpaceCreateDeviceRGB();
	if (colorSpace == NULL)
	{
		fprintf(stderr, "Error allocating color space\n");
		return NULL;
	}
	
	// Allocate memory for image data. This is the destination in memory
	// where any drawing to the bitmap context will be rendered.
	bitmapData = malloc( bitmapByteCount );
	if (bitmapData == NULL) 
	{
		fprintf (stderr, "Memory not allocated!");
		CGColorSpaceRelease( colorSpace );
		return NULL;
	}
	
	// Create the bitmap context. We want pre-multiplied ARGB, 8-bits per component. Regardless of what the source image format is (CMYK, Grayscale, and so on) it will be converted over to the format specified here by CGBitmapContextCreate.
	context = CGBitmapContextCreate (bitmapData,
									 pixelsWide,
									 pixelsHigh,
									 8,      // bits per component
									 bitmapBytesPerRow,
									 colorSpace,
									 kCGImageAlphaPremultipliedFirst);
	if (context == NULL)
	{
		free(bitmapData);
		fprintf (stderr, "Context not created!");
	}
	
	// Make sure and release colorspace before returning
	CGColorSpaceRelease( colorSpace );
	
	return context;
}

// Result buffer is allocated by CreateGrayScale and needs to be released by caller
bool CreateGrayScale(CGImageRef inImage, UIImageOrientation orientation, uint8_t** result, size_t* resWidth, size_t* resHeight, bool isBinary)
{
	/// TODO
	//CGContextTranslateCTM(cgctx, 0.0, CGImageGetHeight(inImage));
	//CGContextScaleCTM(cgctx, 1.0, -1.0);
	
	// Get image width, height. We'll use the entire image.
    size_t width = CGImageGetWidth(inImage);
    size_t height = CGImageGetHeight(inImage);
	
/* Not upsizing or shrinking at this time
#define UPSIZE_THRESHOLD                        0.8
	float shrinkFactor = 1.0;
    bool needToUpsize = false;
	if ((width * height) > (ocrMaxScanSize * quality)) {
		// Take the square root because we will divide both X and Y by the factor
		shrinkFactor = sqrt ((width * height) / (ocrMaxScanSize * quality));
		DebugLog(@"CreateGrayScale: quality=%.2f - resizing image from width=%ld,height=%ld to width=%0.f,height=%0.f", quality, width, height, width / shrinkFactor, height / shrinkFactor);
		width /= shrinkFactor;
		height /= shrinkFactor;
	
    } else if ((width < (1024.0 * UPSIZE_THRESHOLD)) && (height < (768.0 * UPSIZE_THRESHOLD))) {
        float scaleX = width / 1024.0;
        float scaleY = height / 768.0;
        
        if (scaleX < scaleY) { //more x scaling to do than y (so do the least scaling so x doesnt exceed 1024)
            shrinkFactor = scaleY;

        } else {               //same thing opposite case
            shrinkFactor = scaleX;
        }

        DebugLog(@"CreateGrayScale: quality=%.2f - resizing image from width=%d,height=%d to width=%0.f,height=%0.f", quality, (int)width, (int)height, width / shrinkFactor, height / shrinkFactor);
		width /= shrinkFactor;
		height /= shrinkFactor;
        
        needToUpsize = true;
    
    } else {
		DebugLog(@"CreateGrayScale: quality=%.2f - using original image size width=%d,height=%d", quality, (int)width, (int)height);
	}
    *resizingFactor = shrinkFactor;
*/
	
	CGRect rect = {{0,0},{(float)width,(float)height}};
	
	if (orientation == UIImageOrientationLeft || orientation == UIImageOrientationRight) {
		*resWidth = height;
		*resHeight = width;
	} else {
		*resWidth = width;
		*resHeight = height;
	}
	
	// Create the bitmap context
    CGContextRef cgctx = OCRUtilsCreateARGBBitmapContextWithData(width, height);
    if (cgctx == NULL)
        return NO; // Error creating context
	
    // Draw the image to the bitmap context. Once we draw, the memory allocated for the context for rendering will then contain the raw image data in the specified color space.
    CGContextDrawImage(cgctx, rect, inImage);
	
    // Now we can get a pointer to the image data associated with the bitmap context
    uint8_t *data = (uint8_t*)CGBitmapContextGetData (cgctx);

	*result = (uint8_t*)malloc(width*height);
	
    if (data != NULL) {
		for (int w = 0; w < width; ++w) {
			for (int h = 0; h < height; ++h) {
				int i=(h*(int)width)+w;
				float r=data[(i*4)+1], g=data[(i*4)+2], b=data[(i*4)+3]; 
				uint8_t grayLevel;
                if (isBinary) {
                    grayLevel = (uint8_t)(g + r  + b);
                } else
                    grayLevel= (uint8_t)(g*0.59 + r*0.30 + b*0.11);

				switch (orientation) {
					case UIImageOrientationUp:
                    case UIImageOrientationUpMirrored:
						(*result)[i] = grayLevel;
						break;
					case UIImageOrientationDown:
                    case UIImageOrientationDownMirrored:
						(*result)[((height-h-1)*width)+(width-w-1)] = grayLevel;
						break;
					case UIImageOrientationLeft:
                    case UIImageOrientationLeftMirrored:
						(*result)[((width-w-1)*height)+h] = grayLevel;
						break;
					case UIImageOrientationRight:
                    case UIImageOrientationRightMirrored:
						(*result)[(w*height)+(height-h-1)] = grayLevel;
						break;
				}
			}
		}
    }
	
    // When finished, release the context
    CGContextRelease(cgctx); 
	
	if (data)
		free(data);
	
	return YES;	
} // CreateGrayScale

UIImage *CreateRGBUIImageFromGrayscaleData (uint8_t *data, size_t width, size_t height) {
    // First need to convert 8-bit per pixel data to 32 bit per pixel (RGB + Alpha)
    uint8_t *newData = (uint8_t *)malloc(width * height * 4);
    for (int w = 0; w < width; ++w) {
        for (int h = 0; h < height; ++h) {
            int i=(h*(int)width)+w;
            newData[i*4] = 255;           // Alpha
            newData[i*4+1] = data[i];   // Red
            newData[i*4+2] = data[i];   // Green
            newData[i*4+3] = data[i];   // Blue
        }
    }

//    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL,
//                                                          newData,
//                                                          width*height*4, 
//                                                          NULL);
//
//    CGColorSpaceRef colorSpaceRef = CGColorSpaceCreateDeviceRGB();
//    CGBitmapInfo bitmapInfo = kCGBitmapByteOrderDefault;
//    CGColorRenderingIntent renderingIntent = kCGRenderingIntentDefault;
//    CGImageRef imageRef = CGImageCreate(width,
//                                    height,
//                                    8,  // bits per component
//                                    32, // bits per pixel
//                                    4*width,    // Bytes per row
//                                    colorSpaceRef,
//                                    bitmapInfo,
//                                    provider,NULL,NO,renderingIntent);
//     UIImage *newImage = [UIImage imageWithCGImage:imageRef];
//     CGColorSpaceRelease(colorSpaceRef);
    
    CGContextRef gscr = CGBitmapContextCreate(newData, width, height, 8, width*4, CGColorSpaceCreateDeviceRGB(), kCGImageAlphaPremultipliedFirst);
    CGImageRef gsir = CGBitmapContextCreateImage(gscr);
	
	UIImage *newImage = [[UIImage alloc] initWithCGImage:gsir];

    free(newData);
    CGContextRelease(gscr);
    
    return newImage;
}

bool ReceiptParserProcessImage(UIImage *image, uint8_t **grayScaleImageDataPtr, size_t *grayScaleWidthPtr, size_t *grayScaleHeightPtr, bool quick, bool isBinary) {
    
    CGImageRef img = [image CGImage];
    // Transform image to grayscale
    if (!CreateGrayScale(img, [image imageOrientation], grayScaleImageDataPtr, grayScaleWidthPtr, grayScaleHeightPtr, isBinary)) {
        DebugLog(@"CreateGrayScale failed!\n");
        return false;
    }
    
    if (quick) {
        for (int i=0; i<(*grayScaleWidthPtr)*(*grayScaleHeightPtr); i++) {
            uint8_t val = (*grayScaleImageDataPtr)[i];
            // Apparently our cimage class expects an inverted binary image where a text pixel is white and background is black
            if (val > 128)
                (*grayScaleImageDataPtr)[i] = 0;
            else
                (*grayScaleImageDataPtr)[i] = 255;
                // (*grayScaleImageDataPtr)[i] = 1;
        }
    }
    
    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"savebinary"])
    {
        NSLog(@"ReceiptParserProcessImage: saving binary image");
        saveRawImageToDisk(*grayScaleImageDataPtr, *grayScaleWidthPtr, *grayScaleHeightPtr);
    }

    return true;
}
