//
//  WFScanResultsDelegate.h
//  TestFramework
//
//  Created by Danny Panzer on 8/8/16.
//  Copyright © 2016 DFP Group. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "WFProduct.h"
#import "WFScanResults.h"

@protocol WFScanResultsDelegate <NSObject>

@optional

- (void)didOutputScanResults:(WFScanResults*)results;

- (void)didOutputRawText:(NSString*)rawText;

- (void)didFindSearchTarget:(WFProduct*)searchTarget;

- (void)scanningErrorOccurred:(NSError*)error;

@end
