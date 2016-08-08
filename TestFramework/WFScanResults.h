//
//  WFScanResults.h
//  TestFramework
//
//  Created by Danny Panzer on 8/8/16.
//  Copyright © 2016 DFP Group. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "WFScanOptions.h"

@interface WFScanResults : NSObject

@property (atomic, readonly) WFRetailerId retailerId;

@property (strong, atomic, readonly) NSArray *products;

- (instancetype) initWithRetailerId:(WFRetailerId)retailerId
                           products:(NSArray*)products;

@end
