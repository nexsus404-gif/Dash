#import "AppDelegate.h"
#import "RootViewController.h"
#import "Database.h"
#include <sys/stat.h>

@implementation AppDelegate

- (BOOL)application:(UIApplication *)application
        didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {

    // Создаём директорию для БД
    mkdir("/var/mobile/Library/Dash", 0755);

    // Синхронное открытие БД до UI (~10-30ms на iPhone 6)
    int rc = db_open("/var/mobile/Library/Dash/packages.db");
    if (rc != 0) {
        NSLog(@"[Dash] FATAL: db_open failed: %d", rc);
    }

    self.window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];

    RootViewController *root = [[RootViewController alloc] init];
    UINavigationController *nav = [[UINavigationController alloc]
        initWithRootViewController:root];
    nav.navigationBar.translucent = NO;

    self.window.rootViewController = nav;
    [self.window makeKeyAndVisible];

    return YES;
}

- (void)applicationWillTerminate:(UIApplication *)application {
    db_close();
}

@end
