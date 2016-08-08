//
//  WFProduct.m
//  TestFramework
//
//  Created by Danny Panzer on 8/8/16.
//  Copyright © 2016 DFP Group. All rights reserved.
//

#import "WFProduct.h"

@implementation WFProduct

- (instancetype)init {
    self = [super init];
    if (self) {
        _productNumber = nil;
        _productDescription = nil;
        _productQuantity = 0;
    }
    return self;
}

- (instancetype)initWithProductNumber:(NSString*)productNumber
                   productDescription:(NSString*)productDescription
                      productQuantity:(NSInteger)productQuantity {
    
    self = [super init];
    if (self) {
        _productNumber = productNumber;
        _productDescription = productDescription;
        _productQuantity = productQuantity;
    }
    return self;
}

@end
