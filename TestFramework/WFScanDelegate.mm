//
//  WFScanDelegate.mm
//  Windfall
//
//  Created by Patrick Questembert on 3/18/15.
//  Copyright (c) 2015 Windfall. All rights reserved.
//

#import "WFScanDelegate.h"
#import <MicroBlink/MicroBlink.h>
#import "receiptParser.hh"
#import "ocr.hh"
#import "OCRResults.h"
#import "OCRUtils.h"
#import "OCRUtils.hh"

#import <UIKit/UIImage.h>
#import <GPUImage/GPUImage.h>
#import <opencv2/opencv.hpp>

@interface WFScanDelegate ()

@property (strong, nonatomic) GPUImageAdaptiveThresholdFilter *filter;

@end

@implementation WFScanDelegate

void displayDebugMessage (const char *str) {
#if DEBUG
    NSLog(@"%s", str);
#endif
}

- (void)resetParser {
    ocrparser::resetParser();
}

- (id)init {
    self = [super init];
    if (self) {
        self.filter = [GPUImageAdaptiveThresholdFilter new];
        
        //can't use constant here bc don't want to include Constants.h because this is objc++ and it would break refactoring, modules, other stuff...
        float blurRadius = [[NSUserDefaults standardUserDefaults] floatForKey:@"debug_gpu_blur_radius"];
        self.filter.blurRadiusInPixels = blurRadius;
    }
    return self;
}

- (void)setBlurRadius:(CGFloat)blurRadius {
    self.filter.blurRadiusInPixels = blurRadius;
}

+ (NSString*)whitelistForRetailer:(WFRetailer*)retailer {
    // 2015-10-27 removing quote from withelist, too many Blink errors around that
    NSMutableString *whiteList = [NSMutableString stringWithString:@"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.%-/$&@:"];
    if ((retailer.serverId == RETAILER_OFFICEDEPOT_CODE) || (retailer.serverId == RETAILER_MACYS_CODE)) {
        whiteList = [NSMutableString stringWithString:[whiteList stringByAppendingString:@"ubtoal"]];
    }
// 2016-05-06 cancelling because Blink still messed up and returned "-F" for a '+' and I added an OCR rule for this
//    else if ((retailer.serverId == RETAILER_CVS_CODE) || (retailer.serverId == RETAILER_MACYS_CODE)) {
//        whiteList = [NSMutableString stringWithString:[whiteList stringByAppendingString:@"+"]];
//    }
    return (whiteList);
}

+ (NSArray *)excludedFontsForRetailer:(WFRetailer*)retailer {
    if (retailer.serverId == RETAILER_OFFICEDEPOT_CODE) {
//        PP_OCR_FONT_GILL_SANS
//        PP_OCR_FONT_HYPERMARKET
//        PP_OCR_FONT_OCRA
//        PP_OCR_FONT_OPTIMA
//        PP_OCR_FONT_THE_SANS_MONO_CONDENSED_BLACK
//        PP_OCR_FONT_TRAJAN
//        PP_OCR_FONT_VOLTAIRE

        NSArray *fontArray = @[@(27),@(30),@(40),@(43),@(54),@(58),@(62)];
        return (fontArray);
    }
    return [NSArray array];
}

- (UIImage *)createGPUBinaryImage:(UIImage *)img {
    ReleaseLog(@"Start GPU filter");
    
    UIImage *filteredImage = [self.filter imageByFilteringImage:img];
    //[self.filter imageByFilteringImage:img];
    /*
    GPUImagePicture *stillImageSource = [[GPUImagePicture alloc] initWithImage:img];
    
    [stillImageSource addTarget:self.filter];
    //[stillImageFilter useNextFrameForImageCapture];
    [stillImageSource processImage];
    
    //UIImage *currentFilteredVideoFrame = [stillImageFilter imageFromCurrentFramebuffer];
    */
    ReleaseLog(@"End GPU filter");
    
    [[GPUImageContext sharedFramebufferCache] purgeAllUnassignedFramebuffers];
    
    return filteredImage;
    //return nil;
}

- (NSDictionary *)processMicroBlinkResultsArray:(NSArray*)results
                                      withImage:(UIImage *)img
                                  imageIsBinary:(bool)isBinary
                                    newSequence:(bool)newSequence
                                     retailerId:(NSUInteger)retailerId
                                   serverPrices:(NSArray*)serverPricesArray
                              receiptImageIndex:(int *)receiptImageIndexPtr
                                    statusFlags:(NSDictionary**)statusFlags {

#if DEBUG
    NSTimeInterval startDate = [[NSDate date] timeIntervalSinceReferenceDate];
#endif
    
    NSDictionary *iOSResultsDict;
    
    bool lineGroupingDisabled = [[NSUserDefaults standardUserDefaults] boolForKey:@"disablelinegrouping"];
    
    __block bool debugOCR = [[NSUserDefaults standardUserDefaults] boolForKey:@"showtext"];
    __block NSString *rawResults = [NSMutableString string];
    
    for (PPRecognizerResult* res in results) {
        if ([res isKindOfClass:[PPBlinkOcrRecognizerResult class]]) {
            PPBlinkOcrRecognizerResult* result = (PPBlinkOcrRecognizerResult*)res;
            bool worthCreatingBinaryImage = false;  // For some bogus OCR results we want forgo the time-consuming task of creating a binary image
            bool foundLongWord = false;
            bool foundLongNumber = false;

            //NSLog(@"OCR results are:");
            //NSLog(@"Raw ocr: %@", [result parsedResultForName:@"Raw ocr"]);
            //NSLog(@"Price: %@", [result parsedResultForName:@"Price"]);

            PPOcrLayout* rawOcrObject = [result ocrLayout];
            NSLog(@"Dimensions of rawOcrObject are %@", NSStringFromCGRect([rawOcrObject box]));
            
            // Count total number of characters (including \n at the end of each line). Also compute the average height of letter, we will use this later to map very wide spaces to newlines (so as not to trust Blink's arrangement into lines too much)
            
            int numChars = 0;
            int numDigitsOrLetters = 0;
            float sumHeightsDigitsOrLetters = 0;
            for (int i=0; i<rawOcrObject.blocks.count; i++) {
                PPOcrBlock *curBlock = (PPOcrBlock *)rawOcrObject.blocks[i];
            
                for (int j=0; j<curBlock.lines.count; j++) {
                //[curBlock.lines enumerateObjectsUsingBlock:^(id obj, NSUInteger idx, BOOL *stop) {
                    PPOcrLine *curLine = (PPOcrLine *)(curBlock.lines[j]);
                    
                    numChars += [curLine.chars count]; // No need to make space for a newline, MicroBlink OCR already adds one at the end of each line
                    
                    for (int k=0; k<curLine.chars.count; k++) {
                        PPOcrChar *currentItem = (PPOcrChar*)curLine.chars[k];
                        wchar_t currentChar = currentItem.value;
                        if (((currentChar >= '0') && (currentChar <= '9')) || ((currentChar >= 'A') && (currentChar <= 'Z'))) {
                            numDigitsOrLetters++;
                            sumHeightsDigitsOrLetters += ((currentItem.position.ll.y - currentItem.position.ul.y) + (currentItem.position.lr.y - currentItem.position.ur.y)) / 2;
                        }
                    }
                }
            }
            
            
            float averageHeightLettersOrDigits = 0;
            if (numDigitsOrLetters > 0) averageHeightLettersOrDigits = sumHeightsDigitsOrLetters / numDigitsOrLetters;

#define ANDROID_TEST 0
#if !ANDROID_TEST
            // Now allocate buffer and copy set of returned chars (one more for terminating null char)
            MicroBlinkOCRChar *resultsArray = (MicroBlinkOCRChar *)malloc(sizeof(MicroBlinkOCRChar) * (numChars + 1));
            int index = 0;
            for (int i=0; i<rawOcrObject.blocks.count; i++) {
                PPOcrBlock *curBlock = (PPOcrBlock*)rawOcrObject.blocks[i];
                
                for (int j=0; j<curBlock.lines.count; j++) {
                    PPOcrLine *curLine = (PPOcrLine*)curBlock.lines[j];
                    
                    int numConsecutiveDigits = 0;
                    int consecutiveDigitsNotAllTheSame = false;
                    int numConsecutiveLetters = 0;
                    int consecutiveLettersNotAllTheSame = false;
                    unichar lastChar = 0;
#if DEBUG


                    PPOcrFont lastFont = 0, lastLastFont = 0;
                    unichar lastLastChar = 0;
#endif
                    for (int k=0; k<curLine.chars.count; k++) {
                        PPOcrChar *curChar = (PPOcrChar*)curLine.chars[k];
#if DEBUG
                        if ((k == 1) && (curChar.value == '1') && (lastChar == '3')) {
                            DebugLog(@"");
                        }
                        lastLastFont = lastFont;
                        lastLastChar = lastChar;
                        lastChar = curChar.value;
                        lastFont = curChar.font;
#endif
                        
                        if (lineGroupingDisabled && (curChar.value == 0xa))
                            continue;   // Newlines chars don't add any value when line grouping is disabled
                        
                        if ((curChar.value == ' ') && (k > 0) && (k < curLine.chars.count - 1)) {
                            PPOcrChar *previousItem = (PPOcrChar*)curLine.chars[k-1];
                            PPOcrChar *nextItem = (PPOcrChar*)curLine.chars[k+1];
                            float spaceBetween = nextItem.position.ll.x - previousItem.position.lr.x;
                            if (spaceBetween > averageHeightLettersOrDigits * 4) {
                                curChar.value = 0xa;    // Too wide, break into two lines and let our code handle layout via setBlocks
                                DebugLog(@"Breaking line between [%c] and [%c] on line %d (space = %.0f versus av height %.1f", previousItem.value, nextItem.value, j, spaceBetween, averageHeightLettersOrDigits);
                            }
                        }
//#if DEBUG
//                        if ((([curChar position].ul.y < 25) || ([curChar position].ur.y < 25)) && (([curChar position].ll.y < 35) && ([curChar position].lr.y < 35))) {
//                            DebugLog(@"BLINKTEST: %C [%d,%d - %d,%d]", curChar.value, (int)([curChar position].ul.x), (int)([curChar position].ul.y), (int)([curChar position].lr.x), (int)([curChar position].lr.y));
//                        }
//#endif
                        
                        // Validate that all coordinates fall within the image dimensions
                        if ((curChar.value != 0xa)
                            && (([curChar position].ul.x < 0) || ([curChar position].ul.x > img.size.width)
                             || ([curChar position].ur.x < 0) || ([curChar position].ur.x > img.size.width)
                             || ([curChar position].ll.x < 0) || ([curChar position].ll.x > img.size.width)
                             || ([curChar position].lr.x < 0) || ([curChar position].lr.x > img.size.width)
                             || ([curChar position].ul.y < 0) || ([curChar position].ul.y > img.size.height)
                             || ([curChar position].ur.y < 0) || ([curChar position].ur.y > img.size.height)
                             || ([curChar position].ll.y < 0) || ([curChar position].ll.y > img.size.height)
                             || ([curChar position].lr.y < 0) || ([curChar position].lr.y > img.size.height))) {
                                DebugLog(@"Ignoring frame with illegal coordinates [%d,%d - %d,%d] [IMG w=%d,h=%d]", (int)([curChar position].ll.x), (int)([curChar position].ll.y), (int)([curChar position].ur.x), (int)([curChar position].ur.y), (int)img.size.width, (int)img.size.height);
                                //UIImageWriteToSavedPhotosAlbum(img, self, NULL, nil);
                                free (resultsArray);
                                if (newSequence)
                                    ocrparser::resetParser();
                                return ([NSDictionary dictionary]);
                        }
                        
                        MicroBlinkOCRChar *currentItem = resultsArray + index;
            
                        // Copy coordinates. IMPORTANT: lr / ll are actually the LARGER Y values, because in our model the top of the image is Y=0, so the lower corners of a letter are with the larger Y relative to the top of the letter (that's closer to Y=0)
                        currentItem->ul.x = [curChar position].ul.x;
                        currentItem->ul.y = [curChar position].ul.y;
                        currentItem->ur.x = [curChar position].ur.x;
                        currentItem->ur.y = [curChar position].ur.y;
                        currentItem->ll.x = [curChar position].ll.x;
                        currentItem->ll.y = [curChar position].ll.y;
                        currentItem->lr.x = [curChar position].lr.x;
                        currentItem->lr.y = [curChar position].lr.y;

                        currentItem->value = curChar.value; // Copy character
                        if (debugOCR) {
                            rawResults = [rawResults stringByAppendingFormat:@"%c", curChar.value];
                        }
                        currentItem->quality = (int)curChar.quality;
                        
                        if (!worthCreatingBinaryImage) {
                            if (!foundLongWord && ocrparser::isLetter(curChar.value)) {
                                numConsecutiveDigits = 0;
                                
                                if ((lastChar != 0) && (curChar.value != lastChar))
                                    consecutiveLettersNotAllTheSame = true;
                                
                                numConsecutiveLetters++;
                                lastChar = curChar.value;
                                if ((numConsecutiveLetters >= 3) && consecutiveLettersNotAllTheSame) {
                                    foundLongWord = true;
                                    if (foundLongNumber)
                                        worthCreatingBinaryImage = true;
                                }
                            }
                            else if (!foundLongNumber && ocrparser::isDigit(curChar.value)) {
                                numConsecutiveLetters = 0;
                                 
                                if ((lastChar != 0) && (curChar.value != lastChar))
                                    consecutiveDigitsNotAllTheSame = true;
                                
                                numConsecutiveDigits++;
                                lastChar = curChar.value;
                                if ((numConsecutiveDigits >= 4) && consecutiveDigitsNotAllTheSame) {
                                    foundLongNumber = true;
                                    if (foundLongWord)
                                        worthCreatingBinaryImage = true;
                                }
                          } else {
                                numConsecutiveDigits = numConsecutiveLetters = 0;
                                lastChar = 0;
                                consecutiveLettersNotAllTheSame = false;
                            }
                        } // !worthCreatingBinaryImage
                        
                        if ((index == 0) || (index == 2) || ((currentItem->lr.x - currentItem->ll.x + 1 <- 1) || (currentItem->lr.y - currentItem->ur.y + 1 <= 1)) || (curChar.value > 255))
                            DebugLog(@"#%d: 0x%x quality: %ld [%.0f,%.0f - %.0f,%.0f] w=%.0f,h=%.0f]", index, curChar.value, (long)(curChar.quality), currentItem->ll.x, currentItem->ul.y, currentItem->lr.x-1, currentItem->lr.y-1, (currentItem->lr.x - currentItem->ll.x), (currentItem->lr.y - currentItem->ur.y));
                        if (curChar.value != 0xa)
                            DebugLog(@"#%d: %lc quality: %ld [%.0f,%.0f - %.0f,%.0f] w=%.0f,h=%.0f]", index, curChar.value, (long)(curChar.quality), currentItem->ll.x, currentItem->ul.y,  currentItem->lr.x-1, currentItem->lr.y-1, (currentItem->lr.x - currentItem->ll.x), (currentItem->lr.y - currentItem->ur.y));

                        index++;
                        
                    }

// No need to add newlines, MicroBlink OCR already does
//                    // Add newline at the end of the line
//                    MicroBlinkOCRChar *currentItem = resultsArray + index;
//                    // Copy coordinates
//                    currentItem->ul = (CGPointFloat){0,0};
//                    currentItem->ur = (CGPointFloat){0,0};
//                    currentItem->ll = (CGPointFloat){0,0};
//                    currentItem->lr = (CGPointFloat){0,0};
//                    currentItem->value = L'\n';
//                    currentItem->quality = 0;
//                    
//                    index++;
                }
            }
            
            // Null terminate so caller knows the end of the list
            MicroBlinkOCRChar *currentItem = resultsArray + index;
            currentItem->value = 0;
#endif // !ANDROID_TEST
            
#if BINARYIMAGE_LOG
// Best done via the app's saveframestocamera debug setting
//            if (img != nil) {
//                NSLog(@"processMicroBlinkResultsArray: saving input image");
//                UIImageWriteToSavedPhotosAlbum(img, nil, NULL, nil);
//            }
#endif
            
            // Create binary image?
            uint8_t *binaryImageData = NULL;
            size_t binaryImageWidth = 0;
            size_t binaryImageHeight = 0;
            if ((worthCreatingBinaryImage || isBinary) && (img != nil)) {
                ReleaseLog(@"About to call imgproc");
                if (isBinary) {
                    ReceiptParserProcessImage(img, &binaryImageData, &binaryImageWidth, &binaryImageHeight, true, true);
                } else {
                    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"disableimageprocessing"])
                        ReceiptParserProcessImage(img, &binaryImageData, &binaryImageWidth, &binaryImageHeight, true, false);
                    else
                        ReceiptParserProcessImage(img, &binaryImageData, &binaryImageWidth, &binaryImageHeight, false, false);
                }
                    
                ReleaseLog(@"Done with imgproc");
            }
            
            ReleaseLog(@"About to call processBlinkOCRResults");
            // Call our parser code to parse OCR results
            bool meaningfulFrame = false;
            bool oldData = false;
            bool areWeDone = false;
            int receiptImageIndex = -1;
            unsigned long frameStatus = 0;
            retailer_s retailerParams;
            
#if ANDROID_TEST
            // Hard-coded Home Depot product
            //0 quality: 88 [44,352 - 56,373] w=12,h=22]
            //2 quality: 90 [58,353 - 70,374] w=12,h=22]
            //6 quality: 89 [72,353 - 83,373] w=11,h=21]
            //2 quality: 88 [85,353 - 97,374] w=12,h=22]
            //1 quality: 85 [100,353 - 109,375] w=9,h=23]
            //4 quality: 77 [112,353 - 124,375] w=12,h=23]
            //5 quality: 87 [126,353 - 138,373] w=12,h=21]
            //6 quality: 88 [140,353 - 152,374] w=12,h=22]
            //0 quality: 91 [154,353 - 166,374] w=12,h=22]
            //3 quality: 84 [168,353 - 180,374] w=12,h=22]
            //6 quality: 86 [182,353 - 194,374] w=12,h=22]
            //9 quality: 84 [196,353 - 208,375] w=12,h=23]
            numChars = 12;
            MicroBlinkOCRChar *resultsArray = (MicroBlinkOCRChar *)malloc(sizeof(MicroBlinkOCRChar) * (numChars + 1));
            //0 quality: 88 [44,352 - 56,373] w=12,h=22]
            resultsArray[0].value = '0';
            resultsArray[0].ll.x = 44; resultsArray[0].ll.y = 373;
            resultsArray[0].lr.x = 56; resultsArray[0].lr.y = 373;
            resultsArray[0].ul.x = 44; resultsArray[0].ul.y = 352;
            resultsArray[0].ur.x = 56; resultsArray[0].ur.y = 352;
            
            //2 quality: 90 [58,353 - 70,374] w=12,h=22]
            resultsArray[1].value = '2';
            resultsArray[1].ll.x = 58; resultsArray[1].ll.y = 374;
            resultsArray[1].lr.x = 70; resultsArray[1].lr.y = 374;
            resultsArray[1].ul.x = 70; resultsArray[1].ul.y = 353;
            resultsArray[1].ur.x = 83; resultsArray[1].ur.y = 353;
 
            //6 quality: 89 [72,353 - 83,373] w=11,h=21]
            resultsArray[2].value = '6';
            resultsArray[2].ll.x = 72; resultsArray[2].ll.y = 373;
            resultsArray[2].lr.x = 83; resultsArray[2].lr.y = 373;
            resultsArray[2].ul.x = 72; resultsArray[2].ul.y = 353;
            resultsArray[2].ur.x = 83; resultsArray[2].ur.y = 353;

            //2 quality: 88 [85,353 - 97,374] w=12,h=22]
            resultsArray[3].value = '2';
            resultsArray[3].ll.x = 85; resultsArray[3].ll.y = 373;
            resultsArray[3].lr.x = 97; resultsArray[3].lr.y = 374;
            resultsArray[3].ul.x = 85; resultsArray[3].ul.y = 353;
            resultsArray[3].ur.x = 97; resultsArray[3].ur.y = 354;

            //1 quality: 85 [100,353 - 109,375] w=9,h=23]
            resultsArray[4].value = '1';
            resultsArray[4].ll.x = 100; resultsArray[4].ll.y = 375;
            resultsArray[4].lr.x = 109; resultsArray[4].lr.y = 375;
            resultsArray[4].ul.x = 100; resultsArray[4].ul.y = 353;
            resultsArray[4].ur.x = 109; resultsArray[4].ur.y = 353;

            //4 quality: 77 [112,353 - 124,375] w=12,h=23]
            resultsArray[5].value = '4';
            resultsArray[5].ll.x = 112; resultsArray[5].ll.y = 375;
            resultsArray[5].lr.x = 124; resultsArray[5].lr.y = 375;
            resultsArray[5].ul.x = 112; resultsArray[5].ul.y = 353;
            resultsArray[5].ur.x = 124; resultsArray[5].ur.y = 353;

            //5 quality: 87 [126,353 - 138,373] w=12,h=21]
            resultsArray[6].value = '5';
            resultsArray[6].ll.x = 126; resultsArray[6].ll.y = 373;
            resultsArray[6].lr.x = 138; resultsArray[6].lr.y = 373;
            resultsArray[6].ul.x = 126; resultsArray[6].ul.y = 353;
            resultsArray[6].ur.x = 138; resultsArray[6].ur.y = 353;

            //6 quality: 88 [140,353 - 152,374] w=12,h=22]
            resultsArray[7].value = '6';
            resultsArray[7].ll.x = 140; resultsArray[7].ll.y = 374;
            resultsArray[7].lr.x = 152; resultsArray[7].lr.y = 374;
            resultsArray[7].ul.x = 140; resultsArray[7].ul.y = 353;
            resultsArray[7].ur.x = 152; resultsArray[7].ur.y = 353;
            
            //0 quality: 91 [154,353 - 166,374] w=12,h=22]
            resultsArray[8].value = '0';
            resultsArray[8].ll.x = 154; resultsArray[8].ll.y = 374;
            resultsArray[8].lr.x = 166; resultsArray[8].lr.y = 374;
            resultsArray[8].ul.x = 154; resultsArray[8].ul.y = 353;
            resultsArray[8].ur.x = 166; resultsArray[8].ur.y = 353;
            
            //3 quality: 84 [168,353 - 180,374] w=12,h=22]
            resultsArray[9].value = '3';
            resultsArray[9].ll.x = 168; resultsArray[9].ll.y = 374;
            resultsArray[9].lr.x = 180; resultsArray[9].lr.y = 374;
            resultsArray[9].ul.x = 168; resultsArray[9].ul.y = 353;
            resultsArray[9].ur.x = 180; resultsArray[9].ur.y = 353;
            
            //6 quality: 86 [182,353 - 194,374] w=12,h=22]
            resultsArray[10].value = '6';
            resultsArray[10].ll.x = 182; resultsArray[10].ll.y = 374;
            resultsArray[10].lr.x = 194; resultsArray[10].lr.y = 374;
            resultsArray[10].ul.x = 182; resultsArray[10].ul.y = 353;
            resultsArray[10].ur.x = 194; resultsArray[10].ur.y = 353;
            
            //9 quality: 84 [196,353 - 208,375] w=12,h=23]
            resultsArray[11].value = '9';
            resultsArray[11].ll.x = 196; resultsArray[11].ll.y = 375;
            resultsArray[11].lr.x = 208; resultsArray[11].lr.y = 375;
            resultsArray[11].ul.x = 196; resultsArray[11].ul.y = 353;
            resultsArray[11].ur.x = 208; resultsArray[11].ur.y = 353;
            
            resultsArray[12].value = 0;
            
            binaryImageWidth = 644;
            binaryImageHeight = 680;
            
            ocrparser::MatchVector parsingResults = ocrparser::processBlinkOCRResults(resultsArray, NULL, 0, 0, (int)retailerId, &retailerParams, 0, NULL, NULL, NULL, NULL, NULL);
            vector<simpleMatch> v = ocrparser::exportCurrentOCRResultsVector();
            DebugLog(@"exportCurremtOCRResultsVector: got %d elements, first element: [%s, type=%d, nRects=%d]", (int)v.size(), ocrparser::toUTF8(v[0].text).c_str(), v[0].type, v[0].n);
//            int nItems = 0;
//            simpleMatch* a = ocrparser::exportCurrentOCRResultsArray(&nItems);
//            DebugLog(@"exportCurremtOCRResultsArray: got %d elements, first element: [%s, type=%d, nRects=%d]", nItems, ocrparser::toUTF8(a[0].text).c_str(), a[0].type, a[0].n);
#else // ANDROID_TEST
            
            ocrparser::MatchVector parsingResults = ocrparser::processBlinkOCRResults(resultsArray, binaryImageData, binaryImageWidth, binaryImageHeight, (int)retailerId, &retailerParams, newSequence, &meaningfulFrame, &oldData, &areWeDone, &receiptImageIndex, &frameStatus);

            //ocrparser::MatchVector parsingResults = ocrparser::processBlinkOCRResults(resultsArray, NULL, 0, 0, 21, NULL, NULL, 0, NULL, NULL, NULL, NULL, NULL);
#endif
            
#if DEBUG
            if (areWeDone) {
                NSLog(@"processMicroBlinkResultsArray: WE ARE DONE");
            }
#endif
            
            if (receiptImageIndexPtr != NULL) {
                *receiptImageIndexPtr = receiptImageIndex;
            }
            
            if (binaryImageData != NULL)
                free(binaryImageData);

            // Save frame if meaningful and new info was added or if special setting to save meaningful only is set to false
#if DEBUG
            if (((meaningfulFrame && !oldData)
                    || ([[NSUserDefaults standardUserDefaults] boolForKey:@"saveonlymeaningful"] == NO))
                && [[NSUserDefaults standardUserDefaults] boolForKey:@"saveframestocamera"]) {
                    ReleaseLog(@"Saving frame to camera roll");
                    UIImageWriteToSavedPhotosAlbum(img, self, NULL, nil);
            }
#endif
                        
            if (debugOCR && meaningfulFrame) {
                /* DPDP
                NSString *res = [OCRGlobalResults printMatchesFromMatchVector:(void *)&parsingResults];
                rawResults = [res stringByAppendingString:rawResults];
       
                UIPasteboard *pasteboard = [UIPasteboard generalPasteboard];
                [pasteboard setString:rawResults];
                 */
            }
    
            // Convert to what Danny wants
            vector<scannedProduct> products;
            vector<float> discounts;
            vector<scannedItem> phones;
            float subTotal = -1, total = -1;
            wstring dateString;
            vector<serverPrice> serverPrices;
            if (serverPricesArray != nil) {
                for (NSDictionary *iOSServerPrice in serverPricesArray) {
                    NSString *serverSKU = iOSServerPrice[@"sku"];
                    string SKUStr = [serverSKU cStringUsingEncoding:NSASCIIStringEncoding];
                    wstring SKUString = ocrparser::toUTF32(SKUStr.c_str());
                    float serverPriceValue = [iOSServerPrice[@"serverPrice"] floatValue];
                    serverPrice sp;
                    sp.sku = SKUString;
                    sp.price = serverPriceValue;
                    serverPrices.push_back(sp);
                }
            }
            ocrparser::exportCurrentScanResults (&products, &discounts, &subTotal, &total, &dateString, &phones, serverPrices);
//#if DEBUG
//            if (products.size() > 4) {
//                NSLog(@"exportCurrentScanResults: returning %d products", (int)products.size());
//            }
//#endif
            iOSResultsDict = [OCRGlobalResults buildiOSResultsWithProductsVector:products DiscountsVector:discounts Subtotal:subTotal Total:total Date:dateString AndPhones:phones];

            // Old way using a conversion function from C++ MatchVector straight to iOS dictionaries
            //iOSResultsDict = [OCRGlobalResults buildResultsFromMatchVector:(void *)&parsingResults withRetailerParams:&retailerParams andServerPrices:serverPricesArray];
            
            NSMutableDictionary *statusFlagsTemp = [NSMutableDictionary dictionary];
            
            if (areWeDone)
                statusFlagsTemp[WFScanFlagsDone] = @(YES);
            
            if (meaningfulFrame)
                statusFlagsTemp[WFScanFlagsMeaningful] = @(YES);
            
            if (oldData)
                statusFlagsTemp[WFScanFlagsNoNewData] = @(YES);
            
            [statusFlagsTemp addEntriesFromDictionary:[self createFlagsDictFromStatus:frameStatus]];
            
            if (statusFlags != NULL)
                *statusFlags = statusFlagsTemp;
            
            ReleaseLog(@"Done with call to processBlinkOCRResults");
    
            break;
        }
    }
    
#if DEBUG
    NSTimeInterval endDate = [[NSDate date] timeIntervalSinceReferenceDate];
    NSLog(@"Image processing and OCR took %lu seconds", (unsigned long)(endDate - startDate));
    //[[[UIAlertView alloc] initWithTitle:@"Execution Time" message:[NSString stringWithFormat:@"Image processing and OCR took %lu seconds", (unsigned long)(endDate - startDate)] delegate:self cancelButtonTitle:nil otherButtonTitles:nil] show];
#endif

    return iOSResultsDict;
} // processMicroBlinkResultsArray

- (NSDictionary*)createFlagsDictFromStatus:(NSUInteger)frameStatus {
    NSMutableDictionary *flagsDict = [NSMutableDictionary dictionary];
    
    NSLog(@"Frame bitmap: %lu", (unsigned long)frameStatus);
    
    if (frameStatus & SCAN_STATUS_NODATA)
        flagsDict[WFScanFlagsNoData] = @(YES);

    if (frameStatus & SCAN_STATUS_TOOFAR)
        flagsDict[WFScanFlagsTooFar] = @(YES);
    
    if (frameStatus & SCAN_STATUS_TRUNCATEDRIGHT)
        flagsDict[WFScanFlagsTruncatedRight] = @(YES);
    
    if (frameStatus & SCAN_STATUS_TRUNCATEDLEFT)
        flagsDict[WFScanFlagsTruncatedLeft] = @(YES);
    
    if (frameStatus & SCAN_STATUS_TOOCLOSE)
        flagsDict[WFScanFlagsTooClose] = @(YES);
    
    return flagsDict;
}

- (NSDictionary*)processServerPrices:(NSArray*)serverPrices {
    // Get current combined scan results thus far
    //ocrparser::MatchVector parsingResults = ocrparser::getCurrentOCRResults();
    //retailer_t retailerParams = ocrparser::getCurrentRetailersParams();
//#if DEBUG
//    NSMutableArray *newArray = [NSMutableArray arrayWithArray:serverPrices];
//    for (int i=0; i<newArray.count; i++) {
//        NSMutableDictionary *m = newArray[i];
//        NSString *pNumber = m[@"sku"];
//        if ([pNumber isEqualToString:@"062230309"]) {
//            [newArray removeObject:m];
//            break;
//        }
//    }
//    // Add one more synthetic entry to test something
//    NSMutableDictionary *newEntry = [NSMutableDictionary dictionary];
//    newEntry[@"sku"] = @"062230309";
//    newEntry[@"serverPrice"] = [NSNumber numberWithFloat:17.99];
//
//    [newArray addObject:newEntry];
//    
//    return [OCRGlobalResults buildResultsFromMatchVector:(void *)&parsingResults withRetailerParams:&retailerParams andServerPrices:newArray];
//#else
    vector<scannedProduct> products;
    vector<float> discounts;
    float subTotal = -1, total = -1;
    wstring dateString;
    vector<scannedItem> phones;
    vector<serverPrice> serverPricesVector;
    if (serverPrices != nil) {
        for (NSDictionary *iOSServerPrice in serverPrices) {
            NSString *serverSKU = iOSServerPrice[@"sku"];
            string SKUStr = [serverSKU cStringUsingEncoding:NSASCIIStringEncoding];
            wstring SKUString = ocrparser::toUTF32(SKUStr.c_str());
            float serverPriceValue = [iOSServerPrice[@"serverPrice"] floatValue];
            serverPrice sp;
            sp.sku = SKUString;
            sp.price = serverPriceValue;
            serverPricesVector.push_back(sp);
        }
    }
    ocrparser::exportCurrentScanResults (&products, &discounts, &subTotal, &total, &dateString, &phones, serverPricesVector);
    return ([OCRGlobalResults buildiOSResultsWithProductsVector:products DiscountsVector:discounts Subtotal:subTotal Total:total Date:dateString AndPhones:phones]);
    
    //return [OCRGlobalResults buildResultsFromMatchVector:(void *)&parsingResults withRetailerParams:&retailerParams andServerPrices:serverPrices];
//#endif
}

- (UIImage *)fixRotation:(UIImage *)image {
    if (image.imageOrientation == UIImageOrientationUp) return image;
    CGAffineTransform transform = CGAffineTransformIdentity;
    
    switch (image.imageOrientation) {
        case UIImageOrientationDown:
        case UIImageOrientationDownMirrored:
            transform = CGAffineTransformTranslate(transform, image.size.width, image.size.height);
            transform = CGAffineTransformRotate(transform, M_PI);
            break;
            
        case UIImageOrientationLeft:
        case UIImageOrientationLeftMirrored:
            transform = CGAffineTransformTranslate(transform, image.size.width, 0);
            transform = CGAffineTransformRotate(transform, M_PI_2);
            break;
            
        case UIImageOrientationRight:
        case UIImageOrientationRightMirrored:
            transform = CGAffineTransformTranslate(transform, 0, image.size.height);
            transform = CGAffineTransformRotate(transform, -M_PI_2);
            break;
        case UIImageOrientationUp:
        case UIImageOrientationUpMirrored:
            break;
    }
    
    switch (image.imageOrientation) {
        case UIImageOrientationUpMirrored:
        case UIImageOrientationDownMirrored:
            transform = CGAffineTransformTranslate(transform, image.size.width, 0);
            transform = CGAffineTransformScale(transform, -1, 1);
            break;
            
        case UIImageOrientationLeftMirrored:
        case UIImageOrientationRightMirrored:
            transform = CGAffineTransformTranslate(transform, image.size.height, 0);
            transform = CGAffineTransformScale(transform, -1, 1);
            break;
        case UIImageOrientationUp:
        case UIImageOrientationDown:
        case UIImageOrientationLeft:
        case UIImageOrientationRight:
            break;
    }
    
    // Now we draw the underlying CGImage into a new context, applying the transform
    // calculated above.
    CGContextRef ctx = CGBitmapContextCreate(NULL, image.size.width, image.size.height,
                                             CGImageGetBitsPerComponent(image.CGImage), 0,
                                             CGImageGetColorSpace(image.CGImage),
                                             CGImageGetBitmapInfo(image.CGImage));
    CGContextConcatCTM(ctx, transform);
    switch (image.imageOrientation) {
        case UIImageOrientationLeft:
        case UIImageOrientationLeftMirrored:
        case UIImageOrientationRight:
        case UIImageOrientationRightMirrored:
            // Grr…
            CGContextDrawImage(ctx, CGRectMake(0,0,image.size.height,image.size.width), image.CGImage);
            break;
            
        default:
            CGContextDrawImage(ctx, CGRectMake(0,0,image.size.width,image.size.height), image.CGImage);
            break;
    }
    
    // And now we just create a new UIImage from the drawing context
    CGImageRef cgimg = CGBitmapContextCreateImage(ctx);
    UIImage *img = [UIImage imageWithCGImage:cgimg];
    CGContextRelease(ctx);
    CGImageRelease(cgimg);
    return img;
}

- (NSArray *)retailersForBarcode:(NSString *)barcode {
    string barcodeString = [barcode cStringUsingEncoding:NSASCIIStringEncoding];
    wstring barcodeWString = ocrparser::toUTF32(barcodeString.c_str());
    vector<int> retailers = ocrparser::retailersForBarcode(barcodeWString);
    
    NSMutableArray *possibleRetailers = [NSMutableArray array];
    for (int i=0; i<retailers.size(); i++) {
        int retailerID = retailers[i];
        [possibleRetailers addObject:[NSNumber numberWithInt:retailerID]];
    }
    return possibleRetailers;
}

+ (int)validateFuzzyResults:(NSArray *)fuzzyTextArray index0Score:(float)score forScannedText:(NSString *)inputText isExactMatch:(bool *)exact {
    vector<wstring> fuzzyTextVector;
    for (NSString *fuzzyText in fuzzyTextArray) {
        string fuzzyStr = [fuzzyText cStringUsingEncoding:NSASCIIStringEncoding];
        wstring fuzzyString = ocrparser::toUTF32(fuzzyStr.c_str());
        fuzzyTextVector.push_back(fuzzyString);
    }
    string inputStr = [inputText cStringUsingEncoding:NSASCIIStringEncoding];
    wstring inputString = ocrparser::toUTF32(inputStr.c_str());
    return (ocrparser::validateFuzzyResults(fuzzyTextVector, score, inputString, exact));
    //NSMutableString *resNSString = [[NSMutableString alloc] initWithCString:ocrparser::toUTF8(resultString).c_str()
}

- (int) getBlurryScoreForImage:(UIImage*)image {
    
    cv::Mat matImage = [self cvMatFromUIImage:image];
    
    cv::Mat matImageGrey;
    cv::cvtColor(matImage, matImageGrey, CV_BGRA2GRAY);
    matImage.release();
    
    cv::Mat laplacianImage;
    cv::Laplacian(matImageGrey, laplacianImage, CV_8U); // CV_8U
    matImageGrey.release();
    
    cv::Scalar mean, stddev;
    
    meanStdDev(laplacianImage, mean, stddev, cv::Mat());
    laplacianImage.release();
    
    int variance = stddev.val[0] * mean.val[0];
    
    return variance;
}

- (cv::Mat)cvMatFromUIImage:(UIImage *)image
{
    CGColorSpaceRef colorSpace = CGImageGetColorSpace(image.CGImage);
    CGFloat cols = image.size.width;
    CGFloat rows = image.size.height;
    
    cv::Mat cvMat(rows, cols, CV_8UC4); // 8 bits per component, 4 channels (color channels + alpha)
    
    CGContextRef contextRef = CGBitmapContextCreate(cvMat.data,                 // Pointer to  data
                                                    cols,                       // Width of bitmap
                                                    rows,                       // Height of bitmap
                                                    8,                          // Bits per component
                                                    cvMat.step[0],              // Bytes per row
                                                    colorSpace,                 // Colorspace
                                                    kCGImageAlphaNoneSkipLast |
                                                    kCGBitmapByteOrderDefault); // Bitmap info flags
    
    CGContextDrawImage(contextRef, CGRectMake(0, 0, cols, rows), image.CGImage);
    CGContextRelease(contextRef);
    
    return cvMat;
}

- (NSArray*)getEdgesForImage:(UIImage*)image outContentPercent:(float*)contentPercent {
    float minEdgeHeight = 200.f;
    
    std::vector<std::vector<cv::Point> > contours;
    
    cv::Mat matImage = [self cvMatFromUIImage:image];
    
    cv::Mat matImageGrey;
    cv::cvtColor(matImage, matImageGrey, CV_BGRA2GRAY);
    matImage.release();
    
    cv::Mat edges;
    cv::Canny(matImageGrey, edges, 50, 150);
    matImageGrey.release();
    
    
    cv::findContours(edges, contours, CV_RETR_LIST, CV_CHAIN_APPROX_SIMPLE);
    
    double maxHeight = 0, minX = 9999, maxX = 0;
    std::vector<cv::Point> biggest;
    
    NSMutableArray *edgesArray = [NSMutableArray array];
    
    for (size_t i = 0; i < contours.size(); i++)
    {
        
        // approximate contour with accuracy proportional
        // to the contour perimeter
        std::vector<cv::Point> approx;
        approxPolyDP(cv::Mat(contours[i]), approx, arcLength(cv::Mat(contours[i]), true)*0.02, true);
        
        //NSLog(@"Approx poly");
        double minY = approx[0].y;
        double maxY = minY;
        for (int p = 0; p < approx.size(); p++) {
            //NSLog(@"x: %d y: %d", approx[p].x, approx[p].y);
            
            if (approx[p].y < minY) {
                minY = approx[p].y;
            }
            
            if (approx[p].y > maxY) {
                maxY = approx[p].y;
            }
        }
        
        
        double height = maxY - minY;
        //NSLog(@"Poly height: %f", height);
        
        
        if (height > maxHeight) {
            biggest = approx;
            maxHeight = height;
        }
        
        if (height >= minEdgeHeight) {
            
            double totalX = 0;
            
            NSMutableArray *points = [NSMutableArray array];
            for (int p = 0; p < approx.size(); p++) {
                
                CGPoint point = CGPointMake(approx[p].x, approx[p].y);
                [points addObject:[NSValue valueWithCGPoint:point]];
                
                totalX += approx[p].x;
            }
            
            double avgX = totalX / approx.size();
            
            
            if (avgX < minX) {
                minX = avgX;
            }
            
            if (avgX > maxX) {
                maxX = avgX;
            }
            
            [edgesArray addObject:points];
        }

    }
    
    *contentPercent = (maxX - minX) / image.size.width * 100;
    
    return edgesArray;
}

double angle(cv::Point p1, cv::Point p2, cv::Point p0 ) {
    double dx1 = p1.x - p0.x;
    double dy1 = p1.y - p0.y;
    double dx2 = p2.x - p0.x;
    double dy2 = p2.y - p0.y;
    return (dx1 * dx2 + dy1 * dy2) / sqrt((dx1 * dx1 + dy1 * dy1) * (dx2 * dx2 + dy2 * dy2) + 1e-10);
}

@end