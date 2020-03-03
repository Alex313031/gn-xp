// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "app/SceneDelegate.h"

#import "app/parser/Sanitizer-Swift.h"

const NSString* JSONs[3] = {

    @"   {\"a\" : [\"\\true\", false, null, \"\", true, "
    @"3.141592653589793238462643383279 ], \"\\uD834\\uDD1E\": {}}  ",

    @"{\n\
  \"Image\": {\n\
      \"Width\":  800,\n\
      \"Height\": "
    @"600,\n\
      \"Title\":  \"View from 15th Floor\",\n\
      "
    @"\"Thumbnail\": {\n\
          \"Url\":    "
    @"\"http://www.example.com/image/481989943\",\n\
          \"Height\": "
    @"125,\n\
          \"Width\":  100\n\
      },\n\
      \"Animated\" : "
    @"false,\n\
      \"IDs\": [116, 943, 234, 38793]\n\
    }\n\
}",

    @"[\n\
  {\n\
     \"precision\": \"zip\",\n\
     \"Latitude\":  "
    @"1e-7,\n\
     \"Longitude\": -122.3959e3,\n\
     \"Address\":   "
    @"\"\",\n\
     \"City\":      \"SAN FRANCISCO\",\n\
     \"State\":     "
    @"\"CA\",\n\
     \"Zip\":       \"94107\",\n\
     \"Country\":   "
    @"\"US\"\n\
  },\n\
  {\n\
     \"precision\": \"zip\",\n\
     "
    @"\"Latitude\":  37.371991e-17,\n\
     \"Longitude\": -122.026020e+1,\n\
 "
    @"    \"Address\":   \"\",\n\
     \"City\":      \"SUNNYVALE\",\n\
     "
    @"\"State\":     \"CA\",\n\
     \"Zip\":       \"94085\",\n\
     "
    @"\"Country\":   \"US\"\n\
  }\n\
]"};

@implementation SceneDelegate

- (void)scene:(UIScene*)scene
    willConnectToSession:(UISceneSession*)session
                 options:(UISceneConnectionOptions*)connectionOptions {
}

- (void)sceneDidDisconnect:(UIScene*)scene {
}

- (void)sceneDidBecomeActive:(UIScene*)scene {
  for (int i = 0; i < 3; i++) {
    NSLog(@"%@", [[[Sanitizer alloc] init] sanitizeWithJSON:JSONs[i]]);
  }
}

- (void)sceneWillResignActive:(UIScene*)scene {
}

- (void)sceneWillEnterForeground:(UIScene*)scene {
}

- (void)sceneDidEnterBackground:(UIScene*)scene {
}

@end
