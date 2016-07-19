//
//  CustomView.m
//  TestFramework
//
//  Created by Danny Panzer on 7/18/16.
//  Copyright © 2016 DFP Group. All rights reserved.
//

#import "CustomView.h"

@implementation CustomView

- (IBAction)closeButtonClicked:(id)sender {
    [self removeFromSuperview];
}

@end
