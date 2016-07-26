//
//  ScanManager.h
//  TestFramework
//
//  Created by Danny Panzer on 7/18/16.
//  Copyright © 2016 DFP Group. All rights reserved.
//

#import "ScanManager.h"
#import "CustomView.h"
#import "WFScanDelegate.h"

@import UIAlertView_Blocks;
#import <Microblink/MicroBlink.h>
#import <GPUImage/GPUImage.h>
#import <AFNetworking/AFNetworking.h>

@interface ScanManager() <PPCoordinatorDelegate>

@property (strong, atomic) WFScanDelegate *scanDelegate;
@property (strong, atomic) UIImage *binaryImage;
@property (strong, atomic) PPCoordinator *blinkCoordinator;

@property (copy) void(^delegateBlock)(NSDictionary*, NSError*);

@end

@implementation ScanManager

@synthesize blinkLicenseKey = _blinkLicenseKey;

+ (instancetype) sharedManager {
    static ScanManager *sharedManager = nil;
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
    
    [self ocrImage:cardImage withCompletion:nil];
}

- (void)ocrImage:(UIImage*)img withCompletion:(void(^)(NSDictionary*, NSError*))completion {
    
    self.delegateBlock = completion;
    
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        
        NSLog(@"Starting GPU");
        
        self.binaryImage = [self.scanDelegate createGPUBinaryImage:img];
        
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
        
        NSDictionary *parseResults = [self.scanDelegate processMicroBlinkResultsArray:results
                                                                            withImage:self.binaryImage
                                                                        imageIsBinary:YES
                                                                          newSequence:YES
                                                                           retailerId:21
                                                                         serverPrices:nil
                                                                    receiptImageIndex:&receiptImageIndex
                                                                          statusFlags:&statusFlags];
        
        if (self.delegateBlock) {
            
            dispatch_async(dispatch_get_main_queue(), ^{
                self.delegateBlock(parseResults, nil);
            });
            
        }
    });
    
}

- (void)coordinator:(PPCoordinator *)coordinator invalidLicenseKeyWithError:(NSError*)error {
    NSLog(@"License error: %@", error);
}

@end