//
//  OCRUtils.h
//  Windfall
//
//  Created by Patrick Questembert on 4/1/15.
//  Copyright (c) 2015 Windfall. All rights reserved.
//

#if !CODE_CPP
#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIImage.h>

bool OCRUtilsOverlappingX(CGRect r1, CGRect r2);
bool CreateGrayScale(CGImageRef inImage, UIImageOrientation orientation, uint8_t** result, size_t* resWidth, size_t* resHeight, bool isBinary);
bool ReceiptParserProcessImage(UIImage *image, uint8_t **grayScaleImageDataPtr, size_t *grayScaleWidthPtr, size_t *grayScaleHeightPtr, bool quick, bool isBinary);
UIImage *CreateRGBUIImageFromGrayscaleData (uint8_t *data, size_t width, size_t height);
#endif

void *getOBJCMatches(void *ptr);

