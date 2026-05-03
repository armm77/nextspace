/*
 * PrefController.m created by probert on 2001-11-15 20:24:08 +0000
 *
 * Project ImageViewer
 *
 * Created with ProjectCenter - http://www.projectcenter.ch
 *
 * $Id: PrefController.m,v 1.4 2002/01/26 17:08:43 probert Exp $
 */

#import "PrefController.h"
#import "ImageCache.h"

@implementation PrefController

#pragma mark - Init / Dealloc

- (id)init
{
  if ((self = [super init]) == nil)
    return nil;

  if (![NSBundle loadNibNamed:@"Preferences" owner:self]) {
    NSLog(@"PrefController: could not load Preferences.gorm");
    RELEASE(self);
    return nil;
  }

  [window setReleasedWhenClosed:NO];
  [window setFrameAutosaveName:@"Preferences"];

  return self;
}

- (void)dealloc
{
  RELEASE(window);
  [super dealloc];
}

#pragma mark - Show panel

- (void)show
{
  [self _loadPreferencesIntoUI];
  [window setDocumentEdited:NO];

  if (![window isVisible])
    [window setFrameUsingName:@"Preferences"];

  [window makeKeyAndOrderFront:nil];
}

#pragma mark - Private helpers

- (void)_loadPreferencesIntoUI
{
  NSUserDefaults *ud = [NSUserDefaults standardUserDefaults];
  NSString *cacheSize = [ud objectForKey:@"CacheSize"] ?: @"50";
  NSString *openRec   = [ud objectForKey:@"OpenRec"]   ?: @"NO";

  [cacheSizeField setStringValue:cacheSize];

  NSButton *chk = openRecursive
                    ? openRecursive
                    : [self _findCheckboxInView:[window contentView]];
  [chk setState:([openRec isEqualToString:@"YES"]) ? NSOnState : NSOffState];
}

/* Locate the checkbox by title — fallback when Gorm outlet is not wired */
- (NSButton *)_findCheckboxInView:(NSView *)view
{
  for (NSView *sub in [view subviews]) {
    if ([sub isKindOfClass:[NSButton class]]) {
      NSButton *btn = (NSButton *)sub;
      if ([[btn title] rangeOfString:@"recursively"
                             options:NSCaseInsensitiveSearch].location != NSNotFound) {
        return btn;
      }
    }
    NSButton *found = [self _findCheckboxInView:sub];
    if (found) return found;
  }
  return nil;
}

#pragma mark - IBActions

- (IBAction)setCacheSize:(id)sender
{
  [window setDocumentEdited:YES];
}

- (IBAction)setOpenRecursive:(id)sender
{
  [window setDocumentEdited:YES];
}

- (IBAction)resetPreferences:(id)sender
{
  [cacheSizeField setStringValue:@"50"];
  NSButton *chk = openRecursive
                    ? openRecursive
                    : [self _findCheckboxInView:[window contentView]];
  [chk setState:NSOffState];
  [window setDocumentEdited:YES];
}

- (IBAction)setPreferences:(id)sender
{
  NSUserDefaults *ud = [NSUserDefaults standardUserDefaults];
  NSString *sizeStr  = [cacheSizeField stringValue];
  int cacheSize      = [sizeStr intValue];

  if (cacheSize <= 0) {
    NSRunAlertPanel(@"Invalid value",
                    @"Cache size must be a number greater than zero.",
                    @"OK", nil, nil);
    NSString *saved = [ud objectForKey:@"CacheSize"];
    [cacheSizeField setStringValue:(saved) ? saved : @"50"];
    return;
  }

  [ud setObject:sizeStr forKey:@"CacheSize"];
  [[ImageCache sharedCache] setMaxImages:(unsigned int)cacheSize];

  NSButton *chk = openRecursive
                    ? openRecursive
                    : [self _findCheckboxInView:[window contentView]];
  NSString *openRec = (chk && [chk state] == NSOnState) ? @"YES" : @"NO";
  [ud setObject:openRec forKey:@"OpenRec"];

  [ud synchronize];

  [window setDocumentEdited:NO];
  [window orderOut:nil];
}

@end
