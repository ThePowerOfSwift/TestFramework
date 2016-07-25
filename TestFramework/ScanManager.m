//
//  ScanManager.h
//  TestFramework
//
//  Created by Danny Panzer on 7/18/16.
//  Copyright © 2016 DFP Group. All rights reserved.
//

#import "ScanManager.h"
#import "CustomView.h"

#import <UIAlertView+Blocks/UIAlertView+Blocks.h>

@interface ScanManager()

@property (nonatomic) BOOL isEnabled;

@end

@implementation ScanManager

+ (instancetype) sharedManager {
    static ScanManager *sharedManager = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedManager = [[[self class] alloc] init];
    });
    return sharedManager;
}

/*
-(void) showMessageInViewController:(UIViewController *)viewController {
    if (_isEnabled) {
        
        NSBundle* frameworkBundle = [NSBundle bundleForClass:[self class]];
        CustomView *csView = [[frameworkBundle loadNibNamed:@"CustomView" owner:self options:nil] firstObject];
        csView.frame = CGRectMake(0, 0, [[UIScreen mainScreen] bounds].size.width, [[UIScreen mainScreen] bounds].size.height);
        [viewController.view addSubview:csView];
        
        [UIAlertView showWithTitle:@"" message:@"Hide this?" cancelButtonTitle:@"No" otherButtonTitles:@[@"Yes"] tapBlock:^(UIAlertView * _Nonnull alertView, NSInteger buttonIndex) {
            if (buttonIndex != alertView.cancelButtonIndex) {
                [csView removeFromSuperview];
            }
        }];
    }
}
*/

@end