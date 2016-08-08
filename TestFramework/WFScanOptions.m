//
//  WFScanOptions.m
//  TestFramework
//
//  Created by Danny Panzer on 8/8/16.
//  Copyright © 2016 DFP Group. All rights reserved.
//

#import "WFScanOptions.h"

@implementation WFScanOptions

- (instancetype)init {
    self = [super init];
    if (self) {
        self.searchTargets = nil;
        self.retailerId = WFRetailerUnknown;
        self.returnRawText = NO;
        self.returnResults = YES;
    }
    return self;
}

@end
