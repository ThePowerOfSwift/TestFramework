//
//  WFRetailer.h
//  Windfall
//
//  Created by Danny Panzer on 9/21/15.
//  Copyright © 2015 Windfall. All rights reserved.
//

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSUInteger, WFRetailerId) {
    WFRetailerUnknown = 0,  // Means retailer is not known
    WFRetailerTarget = 21,
    WFRetailerWalmart = 25,
    WFRetailerToysRUs = 23,
    WFRetailerBabiesRUs = 27,
    WFRetailerStaples = 20,
    WFRetailerBestBuy = 3,
    WFRetailerOfficeDepot = 31,
    WFRetailerHomeDepot = 22,
    WFRetailerLowes = 28,
    WFRetailerMacys = 12,
    WFRetailerBedBath = 2,
    WFRetailerSears = 18,
    WFRetailerOfficeMax = 15,
    WFRetailerCostco = 29,
    WFRetailerAmazon = 1,
    WFRetailerSamsClub = 99,  // Placeholder
    WFRetailerBJSWholesale = 100, // Placeholder
    WFRetailerKohls = 11,
    WFRetailerKmart = 10,
    WFRetailerWalgreens = 24,
    WFRetailerDuaneReade = 101, // Placeholder
    WFRetailerAlbertsons = 102,  // Placeholder
    WFRetailerCVS = 103,  // Placeholder
    WFRetailerTraderJoes = 104,  // Placeholder
    WFRetailerPetsmart = 105,  // Placeholder
    WFRetailerPetco = 106,  // Placeholder
    WFRetailerFamilyDollar = 107,  // Placeholder
    WFRetailerFredMeyer = 108,  // Placeholder
    WFRetailerMarshalls = 109,  // Placeholder
    WFRetailerWinco = 110  // Placeholder
};

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
