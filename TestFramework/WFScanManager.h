//
//  WFScanManager.h
//  TestFramework
//
//  Created by Danny Panzer on 7/18/16.
//  Copyright © 2016 DFP Group. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "WFScanOptions.h"
#import "WFScanResultsDelegate.h"

@interface WFScanManager : NSObject

+(instancetype) sharedManager;

- (void)ocrTestImage;

- (void)directScanReceiptImage:(UIImage*)img
                    scanOptions:(WFScanOptions*)scanOptions
                   withDelegate:(NSObject<WFScanResultsDelegate>*)delegate;

@property (strong, atomic) NSString *blinkLicenseKey;

@end
