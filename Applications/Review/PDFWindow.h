/*
 * PDFWindow.h
 *
 * PDF viewer window for Review.app using PDFKit 1.2.0
 * Implements the ImageShowing protocol so the existing
 * Inspector works without any modifications.
 */

#import <AppKit/AppKit.h>
#import "ImageShowing.h"

@class PDFDocument;
@class PDFImageRep;

@interface PDFWindow : NSObject <ImageShowing>
{
  id             delegate;

  PDFDocument   *pdfDocument;
  PDFImageRep   *pdfImageRep;

  /* Current state */
  NSString      *pdfPath;
  NSUInteger     currentPage;   /* 1-based, matching PDFKit convention */
  NSUInteger     pageCount;
  double         resolution;    /* dpi — 72 * scaleFactor */

  NSWindow      *window;
  NSImageView   *imageView;
  NSPopUpButton *scalePopup;
  NSButton      *pageUpButton;
  NSButton      *pageDownButton;
  NSTextField   *pageLabel;
}

@property (readonly) NSWindow *window;

/* Designated initialiser — returns nil if the file cannot be opened */
- (id)initWithContentsOfFile:(NSString *)path;

- (id)delegate;
- (void)setDelegate:(id)aDelegate;

/* Page navigation - zoom */
- (void)goToNextPage:(id)sender;
- (void)goToPreviousPage:(id)sender;
- (void)scaleChanged:(id)sender;

/* Window delegate */
- (void)windowWillClose:(NSNotification *)notif;
- (void)windowDidBecomeKey:(NSNotification *)aNotification;

/* ---- ImageShowing protocol ---- */
- (NSString *)path;
- (NSString *)imagePath;
- (NSString *)imageName;

- (NSString *)imageType;
- (NSString *)imageFileSize;
- (NSString *)imageFileModificationDate;
- (NSString *)imageFilePermissions;
- (NSString *)imageFileOwner;

- (NSString *)imageWidth;
- (NSString *)imageHeight;
- (NSString *)imageBitsPerPixel;
- (NSString *)imageBitsPerSample;
- (NSString *)imageNumberOfPlanes;
- (NSString *)imageBytesPerPlane;
- (NSString *)imageBytesPerRow;

- (NSString *)hasAlpha;

@end

@interface NSObject (PDFWindowDelegate)
- (void)imageWindowWillClose:(id)sender;
@end
