#import <Foundation/Foundation.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <Cocoa/Cocoa.h>

extern "C" {
void *cbSetupMetalLayer(void *wnd);
void cbActivateApp(void);
}

void *cbSetupMetalLayer(void *wnd) {
  NSWindow *window = (__bridge NSWindow*)wnd;
  NSView *contentView = [window contentView];
  [contentView setWantsLayer:YES];
  CAMetalLayer *res = [CAMetalLayer layer];
  [contentView setLayer:res];
  return (__bridge void*)res;
}

// macOS only delivers keyboard input to the active application. This process is
// normally started from a terminal, so it must activate itself or the game
// window never becomes key and no key events arrive.
void cbActivateApp(void) {
  [NSApp activateIgnoringOtherApps:YES];
}
