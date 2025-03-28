/* -*- mode: objc -*- */
//
// Project: Preferences
//
// Copyright (C) 2014-2019 Sergii Stoian
//
// This application is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This application is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU General Public
// License along with this library; if not, write to the Free
// Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA.
//

#import <AppKit/NSImage.h>
#import <AppKit/NSBox.h>
#import <AppKit/NSTextField.h>

#import <SystemKit/OSEScreen.h>
#import <SystemKit/OSEPower.h>

#import <Preferences.h>

@class DisplayBox;

@interface ScreenPreferences: NSObject <PrefsModule>
{
  id view;
  id window;
  id canvas;
  id setMainBtn;
  id setStateBtn;
  id applyBtn;
  id revertBtn;

  NSImage *image;

  OSEScreen *systemScreen;
 
  NSMutableArray *displayBoxList;
  DisplayBox     *selectedBox;
  CGFloat scaleFactor;
}

@property (readonly) NSImage *dockImage;
@property (readonly) NSImage *appIconYardImage;
@property (readonly) NSImage *iconYardImage;

- (void)updateDisplayBoxList;

- (void)displayBoxClicked:(DisplayBox *)sender;

@end
