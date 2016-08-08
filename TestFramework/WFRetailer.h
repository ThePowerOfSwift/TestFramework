//
//  WFRetailer.h
//  Windfall
//
//  Created by Danny Panzer on 9/21/15.
//  Copyright © 2015 Windfall. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "WFScanOptions.h"


@interface WFRetailer : NSObject

@property (nonatomic) NSUInteger serverId;
@property (strong, nonatomic) NSString *friendlyName;
@property (nonatomic) BOOL active;
@property (strong, nonatomic) NSString *hexColor;
@property (nonatomic) BOOL dateIsAboveBarcode;
@property (strong, nonatomic) NSString *custSupportPhone;
@property (nonatomic) BOOL textBasedRetailer;

+ (NSArray*)allRetailers;
+ (NSArray*)retailersForStoreList;
+ (WFRetailer*)retailerForId:(NSUInteger)retailerId;

+ (NSString*)barcodeTypeForRetailer:(NSUInteger)retailerId;

- (NSDate*)dateFromBarcode:(NSString*)barcode;
- (NSString*)getEReceiptClaimRefundMessageWithOrderNumber:(NSString*)orderNum;

@end
