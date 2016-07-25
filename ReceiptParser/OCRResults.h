//
//  OCRGlobalResults.h
//  Windfall
//
//  Created by Patrick Questembert on 4/1/15.
//  Copyright (c) 2015 Windfall. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "WFScanDelegate.h"
#include <vector.h>
#import "receiptParser.hh"

@interface OCRGlobalResults : NSObject

+(bool)closeMatchPrice:(float)price toPartialPrice:(NSString *)partialPrice;
+(NSMutableArray *)buildMatchesFromMatchVector:(void *)resultsFromOCR ToMatches:(NSMutableArray *)matches;
+(NSString *)printMatchesFromMatchVector:(void *)resultsFromOCR ;
//+(NSDictionary *)buildResultsFromMatchVector:(void *)resultsFromOCR withRetailerParams:(retailer_t *)retailerParams andServerPrices:(NSArray *)serverPrices;
+(NSDictionary *)buildiOSResultsWithProductsVector:(vector<scannedProduct> &)products DiscountsVector:(vector<float> &)discounts Subtotal:(float)subTotal Total:(float)total Date:(wstring)dateString AndPhones:(vector<scannedItem> &)phones;

@end
