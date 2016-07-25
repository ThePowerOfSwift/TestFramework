//
//  WFRetailer.m
//  Windfall
//
//  Created by Danny Panzer on 9/21/15.
//  Copyright © 2015 Windfall. All rights reserved.
//

#import "WFRetailer.h"

#import <AVFoundation/AVFoundation.h>

static NSArray *allRetailers = nil;

@implementation WFRetailer

+ (NSArray*)allRetailers {
    
    if (!allRetailers) {
        NSMutableArray *tempRetailers = [NSMutableArray array];
        
        WFRetailer *retailer;
        
#if DEBUG
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerUnknown;
        retailer.friendlyName = @"UNKNOWN";
        retailer.active = YES;
        retailer.hexColor = @"ffffff";      
        retailer.custSupportPhone = @"1-800-445-6937";
        [tempRetailers addObject:retailer];
#endif
        
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerTarget;
        retailer.friendlyName = @"Target";
        retailer.active = YES;
        retailer.hexColor = @"cc0000";
        retailer.dateIsAboveBarcode = YES;
        retailer.custSupportPhone = @"1-800-591-3869";
        [tempRetailers addObject:retailer];
        
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerWalmart;
        retailer.friendlyName = @"Walmart";
        retailer.active = YES;
        retailer.hexColor = @"1a75cf";
        retailer.custSupportPhone = @"1-800-966-6546";
        [tempRetailers addObject:retailer];
        
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerToysRUs;
        retailer.friendlyName = @"Toys R Us";
        retailer.active = YES;
        retailer.hexColor = @"004abf";
        retailer.custSupportPhone = @"1-800-869-7787";
        [tempRetailers addObject:retailer];
        
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerBabiesRUs;
        retailer.friendlyName = @"Babies R Us";
        retailer.active = YES;
        retailer.hexColor = @"4a1281";
        retailer.custSupportPhone = @"1-800-869-7787";
        [tempRetailers addObject:retailer];
        
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerStaples;
        retailer.friendlyName = @"Staples";
        retailer.active = YES;
        retailer.hexColor = @"ce0000";
        retailer.custSupportPhone = @"1-800-333-3330";
        [tempRetailers addObject:retailer];
        
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerBestBuy;
        retailer.friendlyName = @"Best Buy";
        retailer.active = YES;
        retailer.hexColor = @"003b64";
        retailer.custSupportPhone = @"1-888-237-8289";
        [tempRetailers addObject:retailer];
        /*
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerOfficeDepot;
        retailer.friendlyName = @"Office Depot";
        retailer.active = YES;
        retailer.hexColor = @"ce0601";
        retailer.custSupportPhone = @"1-800-463-3768";
        [tempRetailers addObject:retailer];
        */
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerHomeDepot;
        retailer.friendlyName = @"Home Depot";
        retailer.active = YES;
        retailer.hexColor = @"f6621f";
        retailer.custSupportPhone = @"1-800-466-3337";
        [tempRetailers addObject:retailer];
        
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerLowes;
        retailer.friendlyName = @"Lowe's";
        retailer.active = YES;
        retailer.hexColor = @"004990";
        retailer.custSupportPhone = @"1-800-445-6937";
        [tempRetailers addObject:retailer];
        
#if DEBUG || 1
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerMacys;
        retailer.friendlyName = @"Macy's";
        retailer.active = YES;
        retailer.hexColor = @"ffffff";                  // Need to change
        retailer.custSupportPhone = @"1-800-445-6937";  // Need actual phone number for Macy's
        [tempRetailers addObject:retailer];
        
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerBedBath;
        retailer.friendlyName = @"BBBY";
        retailer.active = YES;
        retailer.hexColor = @"ffffff";                  // Need to change
        retailer.custSupportPhone = @"1-800-445-6937";  // Need actual phone number for BBBY
        [tempRetailers addObject:retailer];
        
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerSamsClub;
        retailer.friendlyName = @"SAMSCLUB";
        retailer.active = YES;
        retailer.hexColor = @"ffffff";                  // Need to change
        retailer.custSupportPhone = @"1-800-445-6937";  // Need actual phone number for BBBY
        [tempRetailers addObject:retailer];
        
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerCostco;
        retailer.friendlyName = @"COSTCO";
        retailer.active = YES;
        retailer.hexColor = @"ffffff";                  // Need to change
        retailer.custSupportPhone = @"1-800-445-6937";  // Need actual phone number for BBBY
        [tempRetailers addObject:retailer];
        
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerBJSWholesale;
        retailer.friendlyName = @"BJ'S";
        retailer.active = YES;
        retailer.hexColor = @"ffffff";                  // Need to change
        retailer.custSupportPhone = @"1-800-445-6937";  // Need actual phone number for BBBY
        [tempRetailers addObject:retailer];
        
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerKohls;
        retailer.friendlyName = @"KOHL'S";
        retailer.active = YES;
        retailer.hexColor = @"ffffff";                  // Need to change
        retailer.custSupportPhone = @"1-800-445-6937";  // Need actual phone number for BBBY
        [tempRetailers addObject:retailer];
        
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerKmart;
        retailer.friendlyName = @"KMART";
        retailer.active = YES;
        retailer.hexColor = @"ffffff";                  // Need to change
        retailer.custSupportPhone = @"1-800-445-6937";  // Need actual phone number for BBBY
        [tempRetailers addObject:retailer];
        
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerDuaneReade;
        retailer.friendlyName = @"DUANE READE";
        retailer.active = YES;
        retailer.hexColor = @"ffffff";                  // Need to change
        retailer.custSupportPhone = @"1-800-445-6937";  // Need actual phone number for BBBY
        [tempRetailers addObject:retailer];
        
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerWalgreens;
        retailer.friendlyName = @"WALGREENS";
        retailer.active = YES;
        retailer.hexColor = @"ffffff";                  // Need to change
        retailer.custSupportPhone = @"1-800-445-6937";  // Need actual phone number for BBBY
        [tempRetailers addObject:retailer];
        
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerAlbertsons;
        retailer.friendlyName = @"Albertsons";
        retailer.active = YES;
        retailer.hexColor = @"ffffff";                  // Need to change
        retailer.custSupportPhone = @"1-800-445-6937";  // Need actual phone number for BBBY
        [tempRetailers addObject:retailer];
        
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerCVS;
        retailer.friendlyName = @"CVS";
        retailer.active = YES;
        retailer.hexColor = @"ffffff";                  // Need to change
        retailer.custSupportPhone = @"1-800-445-6937";  // Need actual phone number for BBBY
        retailer.textBasedRetailer = YES;
        [tempRetailers addObject:retailer];
        
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerTraderJoes;
        retailer.friendlyName = @"Trader Joe's";
        retailer.active = YES;
        retailer.hexColor = @"ffffff";                  // Need to change
        retailer.custSupportPhone = @"1-800-445-6937";  // Need actual phone number for Trader Joe's
        retailer.textBasedRetailer = YES;
        [tempRetailers addObject:retailer];
        
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerPetsmart;
        retailer.friendlyName = @"Petsmart";
        retailer.active = YES;
        retailer.hexColor = @"ffffff";                  // Need to change
        retailer.custSupportPhone = @"1-800-445-6937";  // Need actual phone number for Trader Joe's
        retailer.textBasedRetailer = YES;
        [tempRetailers addObject:retailer];
        
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerPetco;
        retailer.friendlyName = @"Petco";
        retailer.active = YES;
        retailer.hexColor = @"ffffff";                  // Need to change
        retailer.custSupportPhone = @"1-800-445-6937";  // Need actual phone number for Trader Joe's
        retailer.textBasedRetailer = YES;
        [tempRetailers addObject:retailer];
        
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerFamilyDollar;
        retailer.friendlyName = @"Family Dollar";
        retailer.active = YES;
        retailer.hexColor = @"ffffff";
        retailer.custSupportPhone = @"1-800-445-6937"; 
        retailer.textBasedRetailer = YES;
        [tempRetailers addObject:retailer];
        
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerFredMeyer;
        retailer.friendlyName = @"Fred Meyer";
        retailer.active = YES;
        retailer.hexColor = @"ffffff";
        retailer.custSupportPhone = @"1-800-445-6937";
        retailer.textBasedRetailer = YES;
        [tempRetailers addObject:retailer];
        
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerMarshalls;
        retailer.friendlyName = @"Marshalls";
        retailer.active = YES;
        retailer.hexColor = @"ffffff";
        retailer.custSupportPhone = @"1-800-445-6937";
        retailer.textBasedRetailer = YES;
        [tempRetailers addObject:retailer];
        
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerWinco;
        retailer.friendlyName = @"Winco";
        retailer.active = YES;
        retailer.hexColor = @"ffffff";
        retailer.custSupportPhone = @"1-800-445-6937";
        retailer.textBasedRetailer = YES;
        [tempRetailers addObject:retailer];
#endif
        
        /*
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerSears;
        retailer.friendlyName = @"Sears";
        retailer.active = NO;
        retailer.hexColor = @"24428a";
        [tempRetailers addObject:retailer];
        
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerOfficeMax;
        retailer.friendlyName = @"OfficeMax";
        retailer.active = NO;
        retailer.hexColor = @"facd00";
        [tempRetailers addObject:retailer];
        
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerCostco;
        retailer.friendlyName = @"Costco";
        retailer.active = NO;
        retailer.hexColor = @"005daa";
        [tempRetailers addObject:retailer];
        */
        
        retailer = [WFRetailer new];
        retailer.serverId = WFRetailerAmazon;
        retailer.friendlyName = @"Amazon.com";
        retailer.active = NO;
        retailer.hexColor = @"005daa";
        [tempRetailers addObject:retailer];
        
        
        allRetailers = tempRetailers;
    }
 
    return allRetailers;
}

+ (NSArray*)retailersForStoreList {
    NSPredicate *amazonFilter = [NSPredicate predicateWithBlock:^BOOL(id evaluatedObject, NSDictionary* bindings) {
        WFRetailer *retailer = (WFRetailer*)evaluatedObject;
        return (retailer.serverId != 1);
    }];
    return [[self allRetailers] filteredArrayUsingPredicate:amazonFilter];
}

+ (WFRetailer*)retailerForId:(NSUInteger)retailerId {
    for (WFRetailer *retailer in [self allRetailers]) {
        if (retailer.serverId == retailerId) {
            return retailer;
        }
    }
    return nil;
}

+ (NSString*)barcodeTypeForRetailer:(NSUInteger)retailerId {
    if (retailerId == WFRetailerStaples || retailerId == WFRetailerWalmart) {
        return AVMetadataObjectTypeCode39Code;
    }
    return AVMetadataObjectTypeCode128Code;
}

- (NSDate*)dateFromBarcode:(NSString*)barcode {
    NSCalendar *gregorian = [NSCalendar calendarWithIdentifier:NSCalendarIdentifierGregorian];
    NSInteger year = [gregorian component:NSCalendarUnitYear fromDate:NSDate.date];
    
    if (self.serverId == WFRetailerHomeDepot || self.serverId == WFRetailerOfficeDepot) {
        if (barcode.length >= 4) {
            NSDateFormatter *df = [[NSDateFormatter alloc] init];
            df.dateFormat = @"MMddyyyy HH:mm";
            
            NSString *dateString = [NSString stringWithFormat:@"%@%ld 23:59", [barcode substringToIndex:4], (long)year];
            
            NSDate *candiDate = [df dateFromString:dateString];
            
            //if the date is more than a day in the future, it's probably the wrong year, so decrement
            if ([candiDate timeIntervalSinceNow] > (60 * 60 * 24)) {
                dateString = [NSString stringWithFormat:@"%@%ld 23:59", [barcode substringToIndex:4], (long)(year-1)];
                candiDate = [df dateFromString:dateString];
            }
            
            return candiDate;
        }
    
    } else if (self.serverId == WFRetailerStaples) {
        if (barcode.length >= 10) {
            NSString *dateString = [NSString stringWithFormat:@"%@ 23:59", [barcode substringWithRange:NSMakeRange(4, 6)]];
            NSDateFormatter *df = [[NSDateFormatter alloc] init];
            df.dateFormat = @"MMddyy HH:mm";
            
            return [df dateFromString:dateString];
        }
    
    } else if (self.serverId == WFRetailerLowes) {
        if (barcode.length >= 12) {
            NSString *dateString = [NSString stringWithFormat:@"%@ 23:59", [barcode substringWithRange:NSMakeRange(4, 8)]];
            NSDateFormatter *df = [[NSDateFormatter alloc] init];
            df.dateFormat = @"MMddyyyy HH:mm";
            
            NSLog(@"Date: %@", [df dateFromString:dateString]);
            return [df dateFromString:dateString];
        }
    }
    
    return nil;
}

- (NSString*)getEReceiptClaimRefundMessageWithOrderNumber:(NSString*)orderNum {
    return [NSString stringWithFormat:@"Please call customer service at %@ to claim your refund. Your order number is %@.", self.custSupportPhone, orderNum];
}

@end
