#import <Cocoa/Cocoa.h>
#include "ArduinoPreview.h"

void setup();
void loop();

@interface PreviewView : NSView
- (void)refreshFrame;
@end

@implementation PreviewView {
  preview::Pixels _pixels;
  uint64_t _revision;
  NSBitmapImageRep *_bitmap;
}

- (instancetype)initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _bitmap = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:nullptr
                      pixelsWide:preview::size
                      pixelsHigh:preview::size
                   bitsPerSample:8
                 samplesPerPixel:3
                        hasAlpha:NO
                        isPlanar:NO
                  colorSpaceName:NSDeviceRGBColorSpace
                     bytesPerRow:preview::size * 3
                    bitsPerPixel:24];
    std::fill_n(_bitmap.bitmapData, preview::size * preview::size * 3, 0);
  }
  return self;
}

- (void)refreshFrame {
  if (!preview::copyFrame(_pixels, _revision)) {
    return;
  }
  unsigned char *rgb = _bitmap.bitmapData;
  for (size_t i = 0; i < _pixels.size(); ++i) {
    const uint16_t pixel = _pixels[i];
    rgb[i * 3] = ((pixel >> 11) & 31) * 255 / 31;
    rgb[i * 3 + 1] = ((pixel >> 5) & 63) * 255 / 63;
    rgb[i * 3 + 2] = (pixel & 31) * 255 / 31;
  }
  self.needsDisplay = YES;
}

- (void)drawRect:(NSRect)dirtyRect {
  NSGraphicsContext.currentContext.imageInterpolation = NSImageInterpolationNone;
  [_bitmap drawInRect:self.bounds];
}
@end

@interface PreviewDelegate : NSObject <NSApplicationDelegate>
@end

@implementation PreviewDelegate
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
  return YES;
}
@end

int main() {
  @autoreleasepool {
    preview::spiDelayEnabled = [NSProcessInfo.processInfo.arguments containsObject:@"--spi-delay"];

    NSApplication *app = [NSApplication sharedApplication];
    [app setActivationPolicy:NSApplicationActivationPolicyRegular];

    __attribute__((objc_precise_lifetime)) PreviewDelegate *delegate = [PreviewDelegate new];
    app.delegate = delegate;

    NSMenu *menuBar = [NSMenu new];
    NSMenuItem *appItem = [NSMenuItem new];
    [menuBar addItem:appItem];
    appItem.submenu = [NSMenu new];
    [appItem.submenu addItemWithTitle:@"Quit ESP32 Preview"
                              action:@selector(terminate:) keyEquivalent:@"q"];
    app.mainMenu = menuBar;

    const NSRect frame = NSMakeRect(0, 0, preview::size, preview::size);
    __attribute__((objc_precise_lifetime)) NSWindow *window =
        [[NSWindow alloc] initWithContentRect:frame
                                   styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                     backing:NSBackingStoreBuffered defer:NO];
    window.releasedWhenClosed = NO;
    window.title = preview::spiDelayEnabled ? @"ESP32 Preview · SPI" : @"ESP32 Preview";
    PreviewView *view = [[PreviewView alloc] initWithFrame:frame];
    window.contentView = view;
    [window center];
    [window makeKeyAndOrderFront:nil];
    [app activate];

    // Run the sketch separately so delay() does not block the window.
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
      setup();
      while (true) {
        loop();
        delay(1);
      }
    });
    [NSTimer scheduledTimerWithTimeInterval:1.0 / 60 repeats:YES block:^(NSTimer *timer) {
      [view refreshFrame];
    }];
    [app run];
  }
}
