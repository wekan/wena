/* iOS glue for client/desktop.c: the app's window scene.
 *
 * An app built with the iOS 27 SDK or later must use the UIScene life cycle,
 * or iOS refuses to launch it ("UIScene life cycle is required for apps built
 * with this SDK"). SDL 2.32's UIKit code predates scenes: it makes a plain
 * UIWindow, which a scene-based app does not show. Info.plist names
 * WenaSceneDelegate as the one scene's delegate; it keeps that scene, and
 * desktop.c hands it SDL's window right after creating it, to be shown in it.
 * The scene may connect before or after the window exists: whichever comes
 * second joins them. Everything else - the app delegate, SDL_main, the
 * UIApplication notifications SDL turns into SDL_APP_* events - is SDL's.
 *
 * Objective-C with ARC, built by scripts/build_desktop_ios.sh; the rest of
 * Wena stays C89.
 */
#import <UIKit/UIKit.h>

#include <math.h>

#include "SDL.h"
#include "SDL_syswm.h"

void wena_ios_scene_attach(SDL_Window *window);

static UIWindowScene *wena_scene;
static UIWindow *wena_window;

static void wena_join(void)
{
    if (wena_scene != nil && wena_window != nil && wena_window.windowScene != wena_scene) {
        wena_window.windowScene = wena_scene;
        [wena_window makeKeyAndVisible];
    }
}

void wena_ios_scene_attach(SDL_Window *window)
{
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (window == NULL || !SDL_GetWindowWMInfo(window, &info) || info.subsystem != SDL_SYSWM_UIKIT)
        return;
    wena_window = info.info.uikit.window;
    wena_join();
}

/* The window's safe area in points - clear of the status bar, the notch or
 * Dynamic Island, rounded corners and the home indicator - as insets from
 * each edge. 0 until the window is in its scene. */
int wena_ios_safe_area(int *left, int *top, int *right, int *bottom);

int wena_ios_safe_area(int *left, int *top, int *right, int *bottom)
{
    UIEdgeInsets insets;
    if (wena_window == nil || wena_window.windowScene == nil) return 0;
    insets = wena_window.safeAreaInsets;
    *left = (int)ceil(insets.left);
    *top = (int)ceil(insets.top);
    *right = (int)ceil(insets.right);
    *bottom = (int)ceil(insets.bottom);
    return 1;
}

@interface WenaSceneDelegate : UIResponder <UIWindowSceneDelegate>
@property (strong, nonatomic) UIWindow *window;
@end

@implementation WenaSceneDelegate

- (UIWindow *)window
{
    return wena_window;
}

- (void)setWindow:(UIWindow *)window
{
    /* SDL owns its window. */
}

- (void)scene:(UIScene *)scene willConnectToSession:(UISceneSession *)session
      options:(UISceneConnectionOptions *)connectionOptions
{
    if ([scene isKindOfClass:[UIWindowScene class]]) {
        wena_scene = (UIWindowScene *)scene;
        wena_join();
        /* SDL makes other windows of its own too - a message box's is one: they
         * join the scene when shown. */
        [[NSNotificationCenter defaultCenter]
            addObserverForName:UIWindowDidBecomeVisibleNotification object:nil queue:nil
                    usingBlock:^(NSNotification *note) {
            UIWindow *shown = note.object;
            if ([shown isKindOfClass:[UIWindow class]] && shown.windowScene == nil && wena_scene != nil)
                shown.windowScene = wena_scene;
        }];
    }
}

- (void)sceneDidDisconnect:(UIScene *)scene
{
    if (scene == wena_scene) wena_scene = nil;
}

@end
