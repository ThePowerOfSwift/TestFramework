//
//  WFScanManager.h
//  TestFramework
//
//  Created by Danny Panzer on 7/18/16.
//  Copyright © 2016 DFP Group. All rights reserved.
//

#import "WFScanManager.h"
#import "CustomView.h"
#import "WFScanDelegate.h"
#import "WFScanResults.h"
#import "WFProduct.h"

@import UIAlertView_Blocks;
#import <Microblink/MicroBlink.h>
//#import <Microblink/PPBlinkOcrRecognizerResult.h>

#import <GPUImage/GPUImage.h>
#import <AFNetworking/AFNetworking.h>

@interface WFScanManager() <PPCoordinatorDelegate>

@property (strong, atomic) WFScanDelegate *scanDelegate;
@property (strong, atomic) UIImage *binaryImage;
@property (strong, atomic) PPCoordinator *blinkCoordinator;

@property (strong, atomic) NSObject<WFScanResultsDelegate>* clientDelegate;
@property (strong, atomic) WFScanOptions *clientScanOptions;

@end

@implementation WFScanManager

@synthesize blinkLicenseKey = _blinkLicenseKey;

+ (instancetype) sharedManager {
    static WFScanManager *sharedManager = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedManager = [[[self class] alloc] init];
    });
    return sharedManager;
}

- (id)init {
    self = [super init];
    if (self) {
        self.scanDelegate = [WFScanDelegate new];
        self.clientDelegate = nil;
    }
    return self;
}

- (NSString*)blinkLicenseKey {
    return _blinkLicenseKey;
}

- (void)setBlinkLicenseKey:(NSString *)newKey {
    _blinkLicenseKey = newKey;
    
    PPSettings *settings = [PPSettings new];
    settings.licenseSettings.licenseKey = _blinkLicenseKey;
    
    PPOcrEngineOptions *engineOptions = [PPOcrEngineOptions new];
    engineOptions.minimalLineHeight = 9;   // Was 1
    engineOptions.maximalLineHeight = 150;  // Based on a 1,080 tall vertical image as close as can be to the receipt while still capturing both product and prices. Was 100. PatrickQ 6/22/2016 changing from 42 to 67 because we scan frames at 1,200 width on some phones.
    // PatrickQ 7/22/2016 changing from 67 to 150 because of large font used for total on https://drive.google.com/open?id=0B4jSQhcYsC9VSEV0WmtZVTFJT2s
    engineOptions.maxCharsExpected = 3000;
    engineOptions.colorDropoutEnabled = NO;
    
    engineOptions.imageProcessingEnabled = NO;
    
    engineOptions.lineGroupingEnabled = YES;
    
    
    PPBlinkOcrRecognizerSettings *ocrRecognizerSettings = [[PPBlinkOcrRecognizerSettings alloc] init];
    
    
    PPRawOcrParserFactory *rawOcrFactory = [PPRawOcrParserFactory new];
    rawOcrFactory.options = engineOptions;
    
    [ocrRecognizerSettings addOcrParser:rawOcrFactory name:@"Raw OCR"];
    
    [settings.scanSettings addRecognizerSettings:ocrRecognizerSettings];
    
    self.blinkCoordinator = [[PPCoordinator alloc] initWithSettings:settings delegate:self];
}

- (void)ocrTestImage {
    UIImage *cardImage = [UIImage imageNamed:@"IMG_2924.jpeg"];
    NSLog(@"Image: %@", cardImage);
    
    [self directScanReceiptImage:cardImage
                      scanOptions:nil
                   withDelegate:nil];
}

- (void)directScanReceiptImage:(UIImage*)img
                    scanOptions:(WFScanOptions*)scanOptions
                   withDelegate:(NSObject<WFScanResultsDelegate>*)delegate {
    
    self.clientDelegate = delegate;
    
    if (scanOptions) {
        self.clientScanOptions = scanOptions;
    } else {
        self.clientScanOptions = [WFScanOptions new];
    }
    
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        
        UIImage *imgToBinarize = img;
        
        float width = imgToBinarize.size.width;
        float height = imgToBinarize.size.height;
        float resizingFactorX = width / 720;
        float resizingFactorY = height / 1080;
        float resizingFactor = MAX(resizingFactorX, resizingFactorY);
        
        if (resizingFactor > 1.05) {
            
            NSLog(@"Shrinking image from [%.0f - %.0f] to [%.0f - %.0f] (factor = %.2f", width, height, width / resizingFactor, height / resizingFactor,  resizingFactor);
            
            CGSize newSize = CGSizeMake(width / resizingFactor, height / resizingFactor);
            UIGraphicsBeginImageContextWithOptions(newSize, YES, 1.0); //very impt to pass 1.0 here otherwise on retina devices image will be twice as big as desired
            [imgToBinarize drawInRect:CGRectMake(0, 0, newSize.width, newSize.height)];
            UIImage *resized = UIGraphicsGetImageFromCurrentImageContext();
            UIGraphicsEndImageContext();
            imgToBinarize = resized;
        }
        
        NSLog(@"Starting GPU");
        
        self.binaryImage = [self.scanDelegate createGPUBinaryImage:imgToBinarize];
        
        NSLog(@"End GPU");
        
        dispatch_async(dispatch_get_main_queue(), ^{
            NSLog(@"Starting OCR");
            
            PPImage *blinkImg = [PPImage imageWithUIImage:self.binaryImage];
            
            [self.blinkCoordinator processImage:blinkImg];
        });
        
    });
    
    
}

/*
-(void) showMessageInViewController2:(UIViewController *)viewController {
    
        
    NSBundle* frameworkBundle = [NSBundle bundleForClass:[self class]];
    CustomView *csView = [[frameworkBundle loadNibNamed:@"CustomView" owner:self options:nil] firstObject];
    csView.frame = CGRectMake(0, 0, [[UIScreen mainScreen] bounds].size.width, [[UIScreen mainScreen] bounds].size.height);
    [viewController.view addSubview:csView];
    
    [UIAlertView showWithTitle:@"" message:@"Hide this?" cancelButtonTitle:@"No" otherButtonTitles:@[@"Yes"] tapBlock:^(UIAlertView * _Nonnull alertView, NSInteger buttonIndex) {
        if (buttonIndex != alertView.cancelButtonIndex) {
            [csView removeFromSuperview];
            NSLog(@"Closed");
        }
    }];
    
}
*/

- (void)coordinator:(PPCoordinator *)coordinator didOutputResults:(NSArray<PPRecognizerResult *> *)results {
    NSLog(@"End OCR");
    
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        NSLog(@"Starting Parser");
        
        int receiptImageIndex;
        NSDictionary *statusFlags;
        
        WFRetailerId retailerId = 21;
        
        NSDictionary *parseResults = [self.scanDelegate processMicroBlinkResultsArray:results
                                                                            withImage:self.binaryImage
                                                                        imageIsBinary:YES
                                                                          newSequence:YES
                                                                           retailerId:retailerId
                                                                         serverPrices:nil
                                                                    receiptImageIndex:&receiptImageIndex
                                                                          statusFlags:&statusFlags];
        
        NSMutableArray *prodArray = [NSMutableArray array];
        
        for (NSDictionary *prodDict in parseResults[@"products"]) {
            
            NSString *prodNumber = prodDict[@"sku"][@"value"];
            NSString *prodDescription = prodDict[@"description"][@"value"];
            NSInteger prodQty = [prodDict[@"quantity"] integerValue];
            
            WFProduct *newProduct = [[WFProduct alloc] initWithProductNumber:prodNumber
                                                          productDescription:prodDescription
                                                             productQuantity:prodQty];
            
            [prodArray addObject:newProduct];
        }
        
        WFScanResults *resultsToClient = [[WFScanResults alloc] initWithRetailerId:retailerId products:prodArray];
        
        if (self.clientScanOptions.returnResults) {
            if (self.clientDelegate && [self.clientDelegate respondsToSelector:@selector(didOutputScanResults:)]) {
                
                dispatch_async(dispatch_get_main_queue(), ^{
                    
                    [self.clientDelegate didOutputScanResults:resultsToClient];
                    
                });
                
            }
        }
        
        if (self.clientScanOptions.returnRawText) {
            if (self.clientDelegate && [self.clientDelegate respondsToSelector:@selector(didOutputRawText:)]) {
                
                dispatch_async(dispatch_get_main_queue(), ^{
                    
                    [self.clientDelegate didOutputRawText:[self getRawResults:results]];
                    
                });
                
            }
        }
    });
    
}

- (NSString*)getRawResults:(NSArray<PPRecognizerResult *> *)blinkResults {
    NSMutableString *rawStr = [NSMutableString string];
    
    for (PPRecognizerResult* res in blinkResults) {
        if ([res isKindOfClass:[PPBlinkOcrRecognizerResult class]]) {
            PPBlinkOcrRecognizerResult* result = (PPBlinkOcrRecognizerResult*)res;
            
            PPOcrLayout* rawOcrObject = [result ocrLayout];
            
            for (int i=0; i<rawOcrObject.blocks.count; i++) {
                PPOcrBlock *curBlock = (PPOcrBlock *)rawOcrObject.blocks[i];
                
                for (int j=0; j<curBlock.lines.count; j++) {
                    PPOcrLine *curLine = (PPOcrLine *)(curBlock.lines[j]);
                    
                    for (int k=0; k<curLine.chars.count; k++) {
                        PPOcrChar *curChar = (PPOcrChar*)curLine.chars[k];
                        
                        NSString *curCharStr = [NSString stringWithFormat:@"%C", curChar.value];
                        
                        [rawStr appendString:curCharStr];
                    }
                    
                    [rawStr appendString:@"\n"];
                }
            }
        }
    }
    return rawStr;
}

- (void)coordinator:(PPCoordinator *)coordinator invalidLicenseKeyWithError:(NSError*)error {
    NSLog(@"License error: %@", error);
}

@end