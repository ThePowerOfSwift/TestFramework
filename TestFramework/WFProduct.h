//
//  WFProduct.h
//  TestFramework
//
//  Created by Danny Panzer on 8/8/16.
//  Copyright © 2016 DFP Group. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface WFProduct : NSObject

@property (strong, atomic, readonly) NSString *productNumber;
@property (strong, atomic, readonly) NSString *productDescription;
@property (atomic, readonly) NSInteger productQuantity;

- (instancetype)initWithProductNumber:(NSString*)productNumber
                   productDescription:(NSString*)productDescription
                      productQuantity:(NSInteger)productQuantity;

@end
