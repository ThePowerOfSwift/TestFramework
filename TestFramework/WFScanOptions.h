//
//  WFScanOptions.h
//  TestFramework
//
//  Created by Danny Panzer on 8/8/16.
//  Copyright © 2016 DFP Group. All rights reserved.
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

@interface WFScanOptions : NSObject

@property (atomic) WFRetailerId retailerId;

@property (strong, atomic, nullable) NSArray *searchTargets;

@property (atomic) BOOL returnResults;
@property (atomic) BOOL returnRawText;

@end
