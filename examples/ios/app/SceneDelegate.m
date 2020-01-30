// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "app/SceneDelegate.h"

#import "app/parser/Sanitizer-Swift.h"

@implementation SceneDelegate

- (void)scene:(UIScene*)scene
    willConnectToSession:(UISceneSession*)session
                 options:(UISceneConnectionOptions*)connectionOptions {
}

- (void)sceneDidDisconnect:(UIScene*)scene {
}

- (void)sceneDidBecomeActive:(UIScene*)scene {
  NSLog(@"%@", [[[Sanitizer alloc] init] sanitizeWithJSON:@"{}"]);
}

- (void)sceneWillResignActive:(UIScene*)scene {
}

- (void)sceneWillEnterForeground:(UIScene*)scene {
}

- (void)sceneDidEnterBackground:(UIScene*)scene {
}

@end
