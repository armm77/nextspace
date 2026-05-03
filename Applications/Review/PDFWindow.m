/*
 * PDFWindow.m
 *
 * PDF viewer window for Review.app using PDFKit 1.2.0
 *
 * Rendering pipeline:
 *   PDFDocument  →  PDFImageRep (xpdf engine, RGB bitmap)
 *                →  NSImage  →  NSImageView
 *
 * PDFImageRep works at 72 dpi by default (PDFBaseResolution).
 * Zoom is achieved by multiplying that base by a scale factor
 * and passing the result to -setResolution:.
 */

#import <AppKit/AppKit.h>
#import <PDFKit/PDFDocument.h>
#import <PDFKit/PDFImageRep.h>

#import "PDFWindow.h"

/* Scale steps shown in the popup — same set used by ImageWindow */
static NSString * const kScaleItems[] = {
  @"12.5%", @"25%", @"50%", @"75%", @"100%",
  @"150%", @"200%", @"400%", nil
};
static const double kScaleValues[] = {
  0.125, 0.25, 0.50, 0.75, 1.0, 1.5, 2.0, 4.0
};
static const NSUInteger kDefaultScaleIndex = 4; /* 100% */

/* Base DPI declared by PDFKit (72.0). */
static const double kBaseDPI = 72.0;

/* Minimum window content size */
static const CGFloat kMinWidth  = 400.0;
static const CGFloat kMinHeight = 300.0;
static const CGFloat kToolbarH  = 32.0;   /* height of the bottom toolbar */

/* ------------------------------------------------------------------ */
@implementation PDFWindow

/* ------------------------------------------------------------------ */
#pragma mark - Init / Dealloc

- (id)initWithContentsOfFile:(NSString *)path
{
  self = [super init];
  if (!self) return nil;

  /* 1. Open the PDF document */
  pdfDocument = RETAIN([PDFDocument documentFromFile:path]);
  if (!pdfDocument || ![pdfDocument isOk]) {
    NSLog(@"PDFWindow: cannot open %@  (errorCode=%d)",
          path, pdfDocument ? [pdfDocument errorCode] : -1);
    RELEASE(pdfDocument);
    RELEASE(self);
    return nil;
  }

  pdfPath    = RETAIN(path);
  pageCount  = [pdfDocument pageCount];
  currentPage = 1;
  resolution  = kBaseDPI * kScaleValues[kDefaultScaleIndex]; /* 72 dpi @ 100% */

  /* 2. Create the image rep (one per document, page changed via setPageNum:) */
  pdfImageRep = [[PDFImageRep alloc] initWithDocument:pdfDocument];
  [pdfImageRep setPageNum:(int)currentPage];
  [pdfImageRep setResolution:resolution];

  /* 3. Build the window */
  [self _buildWindow];

  /* 4. Render the first page */
  [self _updateDisplay];

  return self;
}

- (void)dealloc
{
  RELEASE(pdfPath);
  RELEASE(pdfImageRep);
  RELEASE(pdfDocument);
  [super dealloc];
}

/* ------------------------------------------------------------------ */
#pragma mark - Window construction

- (void)_buildWindow
{
  NSScrollView  *scrollView;
  NSView        *toolbar;
  NSRect         contentRect;
  NSSize         pageSize;

  /* Use the first page size (at 100% scale) to set initial window size */
  pageSize = [pdfDocument pageSize:1 considerRotation:YES];

  CGFloat winW = MAX(pageSize.width,  kMinWidth);
  CGFloat winH = MAX(pageSize.height, kMinHeight) + kToolbarH;

  contentRect = NSMakeRect(0, 0, winW, winH);

  window = [[NSWindow alloc]
              initWithContentRect: contentRect
                        styleMask: (NSTitledWindowMask       |
                                    NSClosableWindowMask     |
                                    NSMiniaturizableWindowMask |
                                    NSResizableWindowMask)
                          backing: NSBackingStoreBuffered
                            defer: NO];

  [window setMinSize:NSMakeSize(kMinWidth, kMinHeight + kToolbarH)];
  [window setTitle:[[pdfPath lastPathComponent]
                     stringByDeletingPathExtension]];
  [window setDelegate:(id)self];
  [window setReleasedWhenClosed:NO];

  NSView *content = [window contentView];

  /* ---- Bottom toolbar ---- */
  toolbar = [[NSView alloc]
               initWithFrame:NSMakeRect(0, 0, winW, kToolbarH)];
  [toolbar setAutoresizingMask:(NSViewWidthSizable)];

  /* Page-up button (previous page) */
  pageUpButton = [[NSButton alloc]
                    initWithFrame:NSMakeRect(4, 4, 24, 24)];
  [pageUpButton setImage:[NSImage imageNamed:@"PageUp"]];
  [pageUpButton setAlternateImage:[NSImage imageNamed:@"PageUpH"]];
  [pageUpButton setButtonType:NSMomentaryChangeButton];
  [pageUpButton setBordered:NO];
  [pageUpButton setTarget:self];
  [pageUpButton setAction:@selector(goToPreviousPage:)];
  [toolbar addSubview:pageUpButton];
  RELEASE(pageUpButton);

  /* Page-down button (next page) */
  pageDownButton = [[NSButton alloc]
                      initWithFrame:NSMakeRect(32, 4, 24, 24)];
  [pageDownButton setImage:[NSImage imageNamed:@"PageDown"]];
  [pageDownButton setAlternateImage:[NSImage imageNamed:@"PageDownH"]];
  [pageDownButton setButtonType:NSMomentaryChangeButton];
  [pageDownButton setBordered:NO];
  [pageDownButton setTarget:self];
  [pageDownButton setAction:@selector(goToNextPage:)];
  [toolbar addSubview:pageDownButton];
  RELEASE(pageDownButton);

  /* Page label  "1 / 12" */
  pageLabel = [[NSTextField alloc]
                 initWithFrame:NSMakeRect(60, 6, 90, 20)];
  [pageLabel setBezeled:NO];
  [pageLabel setDrawsBackground:NO];
  [pageLabel setEditable:NO];
  [pageLabel setSelectable:NO];
  [pageLabel setAlignment:NSCenterTextAlignment];
  [toolbar addSubview:pageLabel];
  RELEASE(pageLabel);

  /* Scale popup — right-aligned */
  scalePopup = [[NSPopUpButton alloc]
                  initWithFrame:NSMakeRect(winW - 84, 4, 80, 24)
                      pullsDown:NO];
  [scalePopup setAutoresizingMask:NSViewMinXMargin];
  NSUInteger i = 0;
  while (kScaleItems[i]) {
    [scalePopup addItemWithTitle:kScaleItems[i]];
    i++;
  }
  [scalePopup selectItemAtIndex:kDefaultScaleIndex];
  [scalePopup setTarget:self];
  [scalePopup setAction:@selector(scaleChanged:)];
  [toolbar addSubview:scalePopup];
  RELEASE(scalePopup);

  [content addSubview:toolbar];
  RELEASE(toolbar);

  /* ---- Scroll view + image view ---- */
  NSRect scrollRect = NSMakeRect(0, kToolbarH,
                                 winW, winH - kToolbarH);
  scrollView = [[NSScrollView alloc] initWithFrame:scrollRect];
  [scrollView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
  [scrollView setHasVerticalScroller:YES];
  [scrollView setHasHorizontalScroller:YES];
  [scrollView setBorderType:NSBezelBorder];

  /* Image view sized to first page — will be resized after each render */
  imageView = [[NSImageView alloc]
                 initWithFrame:NSMakeRect(0, 0,
                                          pageSize.width,
                                          pageSize.height)];
  [imageView setImageAlignment:NSImageAlignCenter];
  [imageView setEditable:NO];

  [scrollView setDocumentView:imageView];
  RELEASE(imageView);

  [content addSubview:scrollView];
  RELEASE(scrollView);

  [window center];
  [window makeKeyAndOrderFront:nil];
  [self _updatePageLabel];
}

/* ------------------------------------------------------------------ */
#pragma mark - Rendering

- (void)_updateDisplay
{
  /* Tell PDFImageRep which page and resolution to use */
  [pdfImageRep setPageNum:(int)currentPage];
  [pdfImageRep setResolution:resolution];

  /* Get the scaled page size from the rep */
  NSSize pageSize = [pdfImageRep size];

  /* Create a blank NSImage of the right size, then draw the rep into it
   * using lockFocus so that drawInRect: (and thus _updatePage inside
   * PDFImageRep) is actually called and produces the bitmap.         */
  NSImage *image = [[NSImage alloc] initWithSize:pageSize];
  [image lockFocus];
  [pdfImageRep drawInRect:NSMakeRect(0, 0, pageSize.width, pageSize.height)];
  [image unlockFocus];

  /* Resize the document view to match the rendered page */
  [imageView setFrameSize:pageSize];
  [imageView setImage:image];
  RELEASE(image);

  [self _updatePageLabel];
  [self _updateNavigationButtons];
}

- (void)_updatePageLabel
{
  [pageLabel setStringValue:
    [NSString stringWithFormat:@"%lu / %lu",
     (unsigned long)currentPage,
     (unsigned long)pageCount]];
}

- (void)_updateNavigationButtons
{
  [pageUpButton   setEnabled:(currentPage > 1)];
  [pageDownButton setEnabled:(currentPage < pageCount)];
}

/* ------------------------------------------------------------------ */
#pragma mark - Navigation

- (void)goToNextPage:(id)sender
{
  if (currentPage < pageCount) {
    currentPage++;
    [self _updateDisplay];
  }
}

- (void)goToPreviousPage:(id)sender
{
  if (currentPage > 1) {
    currentPage--;
    [self _updateDisplay];
  }
}

/* ------------------------------------------------------------------ */
#pragma mark - Zoom

- (void)scaleChanged:(id)sender
{
  NSUInteger idx = [scalePopup indexOfSelectedItem];
  if (idx < (sizeof(kScaleValues) / sizeof(kScaleValues[0]))) {
    resolution = kBaseDPI * kScaleValues[idx];
    [self _updateDisplay];
  }
}

/* ------------------------------------------------------------------ */
#pragma mark - Delegate / accessors

- (id)delegate  { return delegate; }
- (void)setDelegate:(id)aDelegate { delegate = aDelegate; }

- (NSWindow *)window { return window; }

- (void)windowWillClose:(NSNotification *)notif
{
  if ([delegate respondsToSelector:@selector(imageWindowWillClose:)]) {
    [delegate imageWindowWillClose:self];
  }
}

- (void)windowDidBecomeKey:(NSNotification *)aNotification
{
  /* Notify the Inspector to update its display for this window */
  [[NSNotificationCenter defaultCenter]
    postNotificationName:@"ImageWindowDidBecomeKey"
                  object:self];
}

/* ------------------------------------------------------------------ */
#pragma mark - ImageShowing protocol

- (NSString *)path       { return pdfPath; }
- (NSString *)imagePath  { return pdfPath; }

- (NSString *)imageName
{
  return [pdfPath lastPathComponent];
}

- (NSString *)imageType
{
  return @"PDF";
}

- (NSString *)imageFileSize
{
  NSDictionary *attrs = [[NSFileManager defaultManager]
                           fileAttributesAtPath:pdfPath
                                   traverseLink:YES];
  if (!attrs) return @"N/A";
  unsigned long long bytes = [[attrs objectForKey:NSFileSize]
                                unsignedLongLongValue];
  if (bytes < 1024)
    return [NSString stringWithFormat:@"%llu B", bytes];
  else if (bytes < 1024 * 1024)
    return [NSString stringWithFormat:@"%.1f KB", bytes / 1024.0];
  else
    return [NSString stringWithFormat:@"%.1f MB", bytes / (1024.0 * 1024.0)];
}

- (NSString *)imageFileModificationDate
{
  NSDictionary *attrs = [[NSFileManager defaultManager]
                           fileAttributesAtPath:pdfPath
                                   traverseLink:YES];
  if (!attrs) return @"N/A";
  NSDate *date = [attrs objectForKey:NSFileModificationDate];
  return date ? [date description] : @"N/A";
}

- (NSString *)imageFilePermissions
{
  NSDictionary *attrs = [[NSFileManager defaultManager]
                           fileAttributesAtPath:pdfPath
                                   traverseLink:YES];
  if (!attrs) return @"N/A";
  NSUInteger perms = [[attrs objectForKey:NSFilePosixPermissions]
                        unsignedIntegerValue];
  return [NSString stringWithFormat:@"%04lo", (unsigned long)perms];
}

- (NSString *)imageFileOwner
{
  NSDictionary *attrs = [[NSFileManager defaultManager]
                           fileAttributesAtPath:pdfPath
                                   traverseLink:YES];
  if (!attrs) return @"N/A";
  return [attrs objectForKey:NSFileOwnerAccountName] ?: @"N/A";
}

- (NSString *)imageWidth
{
  NSSize s = [pdfDocument pageSize:(int)currentPage considerRotation:YES];
  return [NSString stringWithFormat:@"%.0f pt", s.width];
}

- (NSString *)imageHeight
{
  NSSize s = [pdfDocument pageSize:(int)currentPage considerRotation:YES];
  return [NSString stringWithFormat:@"%.0f pt", s.height];
}

/* The following bitmap-specific fields are not applicable to PDF.
   Return N/A so the Inspector shows something sensible. */
- (NSString *)imageBitsPerPixel    { return @"N/A (PDF)"; }
- (NSString *)imageBitsPerSample   { return @"N/A (PDF)"; }
- (NSString *)imageNumberOfPlanes  { return @"N/A (PDF)"; }
- (NSString *)imageBytesPerPlane   { return @"N/A (PDF)"; }
- (NSString *)imageBytesPerRow     { return @"N/A (PDF)"; }
- (NSString *)hasAlpha             { return @"NO"; }

@end
