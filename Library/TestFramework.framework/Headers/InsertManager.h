//
//  InsertManager.h
//  TestFramework
//
//  Created by Danny Panzer on 7/18/16.
//  Copyright © 2016 DFP Group. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@interface InsertManager : NSObject

+(instancetype) sharedManager;

-(void) startManager;
-(void) stopManager;

-(void) showMessageInViewController:(UIViewController *)viewController;

-(BOOL) isManagerRunning;

@end
