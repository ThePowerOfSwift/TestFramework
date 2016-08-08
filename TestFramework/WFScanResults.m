//
//  WFScanResults.m
//  TestFramework
//
//  Created by Danny Panzer on 8/8/16.
//  Copyright © 2016 DFP Group. All rights reserved.
//

#import "WFScanResults.h"

@implementation WFScanResults

- (instancetype)init {
    self = [super init];
    if (self) {
        _retailerId = WFRetailerUnknown;
        _products = nil;
    }
    return self;
}

- (instancetype) initWithRetailerId:(WFRetailerId)retailerId
                           products:(NSArray*)products {
    
    self = [super init];
    if (self) {
        _retailerId = retailerId;
        _products = products;
    }
    return self;
}

@end
