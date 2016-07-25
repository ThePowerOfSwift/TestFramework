//
//  ScanManager.h
//  TestFramework
//
//  Created by Danny Panzer on 7/18/16.
//  Copyright © 2016 DFP Group. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@interface ScanManager : NSObject

+(instancetype) sharedManager;

- (void)doThing;
-(void) showMessageInViewController:(UIViewController *)viewController;

@end
