//
//  ControlsView.m
//  Descent
//
//  Created by Devin Tuchsen on 10/29/15.
//

#import "ControlsView.h"
#include "types.h"
#include "game.h"
#include "key.h"
#include "timer.h"

#define NUM_BUTTONS 25
#define MAX_KEYS 2

#define ACCELERATE_BTN 0
#define REVERSE_BTN 1
#define SLIDE_LEFT_BTN 2
#define SLIDE_RIGHT_BTN 3
#define SLIDE_UP_BTN 4
#define SLIDE_DOWN_BTN 5
#define BANK_LEFT_BTN 6
#define BANK_RIGHT_BTN 7
#define FIRE_PRIMARY_BTN 8
#define FIRE_SECONDARY_BTN 9
#define TOGGLE_PRIMARY_BTN 10
#define TOGGLE_SECONDARY_BTN 11
#define FIRE_FLARE_BTN 12
#define ACCELERATE_SLIDE_LEFT_BTN 13
#define ACCELERATE_SLIDE_RIGHT_BTN 14
#define REVERSE_SLIDE_LEFT_BTN 15
#define REVERSE_SLIDE_RIGHT_BTN 16
#define MENU_BTN 17
#define MAP_BTN 18
#define REAR_VIEW_BTN 19
#define TOGGLE_COCKPIT_BTN 20
#define HEADLIGHT_BTN 21
#define ENERGY_TRANSFER_BTN 22
#define PLACE_MARKER 23
#define GUIDE_BOT_MENU 24

// Contols interface
extern void key_handler(unsigned char scancode, bool down);
extern void mouse_handler(short x, short y, bool down);
extern int touch_dx, touch_dy;

typedef struct GameButton {
	CGRect rect;
	unsigned int keys[MAX_KEYS];
	int nKeys;
	bool activateOnHover;
} GameButton;

@implementation ControlsView {
	GameButton buttons[NUM_BUTTONS];
	NSMutableDictionary *touchButtons;
	NSValue *trackingTouch;
	long accelerateLastTouched;
}

- (BOOL)point:(CGPoint)point inRect:(CGRect)rect {
	return CGRectContainsPoint(rect, point);
}

// TODO: The buttons should align with the screen size; currently hard-coded for iPhone 5 screen
// Buttons are pretty crazy, since we're trying to put desktop 6dof shooter controls on a tiny screen!
- (id)initWithFrame:(CGRect)frame {
	self = [super initWithFrame:frame];
	self.multipleTouchEnabled = YES;
	self.backgroundColor = [UIColor colorWithRed:1.0f green:1.0f blue:1.0f alpha:0.0f];
	accelerateLastTouched = 0;
	touchButtons = [[NSMutableDictionary alloc] init];
	CGFloat width = frame.size.width;
	CGFloat height = frame.size.height;
	
	// Button rects
	CGRect accelerateRect, reverseRect, slideLeftRect, slideRightRect,
	slideUpRect, slideDownRect, bankLeftRect, bankRightRect,
	accelerateSlideLeftRect, accelerateSlideRightRect, reverseSlideLeftRect, reverseSlideRightRect,
	firePrimaryRect, fireSecondaryRect, togglePrimaryRect, toggleSecondaryRect, fireFlareRect,
	menuRect, mapRect, toggleCockpitRect, rearViewRect, headlightRect, energyTransferRect,
	placeMarkerRect, guideBotMenuRect;
	
	// Movement
	accelerateRect = CGRectMake(120, height - 185, 55, 55);
	reverseRect = CGRectMake(120, height - 75, 55, 55);
	slideLeftRect = CGRectMake(65, height - 130, 55, 55);
	slideRightRect = CGRectMake(175, height - 130, 55, 55);
	slideUpRect = CGRectMake(25, height - 185, 35, 80);
	slideDownRect = CGRectMake(25, height - 100, 35, 80);
	bankLeftRect = CGRectMake(65, height - 225, 80, 35);
	bankRightRect = CGRectMake(150, height - 225, 80, 35);
	
	// Combined movement
	accelerateSlideLeftRect = CGRectMake(65, height - 185, 55, 55);
	accelerateSlideRightRect = CGRectMake(175, height - 185, 55, 55);
	reverseSlideLeftRect = CGRectMake(65, height - 75, 55, 55);
	reverseSlideRightRect = CGRectMake(175, height - 75, 55, 55);
	
	// Weapons
	firePrimaryRect = CGRectMake(width - 95, height - 170, 70, 70);
	fireSecondaryRect = CGRectMake(width - 175, height - 90, 70, 70);
	fireFlareRect = CGRectMake(width - 85, height - 80, 50, 50);
	togglePrimaryRect = CGRectMake(width - 95, height - 225, 70, 40);
	toggleSecondaryRect = CGRectMake(width - 230, height - 90, 40, 70);
	
	// Menus
	CGFloat menuSpacing = (width - 250) / 7;
	menuRect = CGRectMake(25, 20, 25, 25);
	toggleCockpitRect = CGRectMake(50 + menuSpacing, 20, 25, 25);
	rearViewRect = CGRectMake(75 + menuSpacing * 2, 20, 25, 25);
	headlightRect = CGRectMake(100 + menuSpacing * 3, 20, 25, 25);
	energyTransferRect = CGRectMake(125 + menuSpacing * 4, 20, 25, 25);
	placeMarkerRect = CGRectMake(150 + menuSpacing * 5, 20, 25, 25);
	guideBotMenuRect = CGRectMake(175 + menuSpacing * 6, 20, 25, 25);
	mapRect = CGRectMake(200 + menuSpacing * 7, 20, 25, 25);
	
	// Define buttons
	buttons[ACCELERATE_BTN] = (struct GameButton){ accelerateRect, { KEY_A }, 1, true };
	buttons[REVERSE_BTN] = (struct GameButton){ reverseRect, { KEY_Z }, 1, true };
	buttons[SLIDE_LEFT_BTN] = (struct GameButton){ slideLeftRect, { KEY_PAD1 }, 1, true };
	buttons[SLIDE_RIGHT_BTN] = (struct GameButton){ slideRightRect, { KEY_PAD3 }, 1, true };
	buttons[SLIDE_UP_BTN] = (struct GameButton){ slideUpRect, { KEY_PADMINUS }, 1, true };
	buttons[SLIDE_DOWN_BTN] = (struct GameButton){ slideDownRect, { KEY_PADPLUS }, 1, true };
	buttons[BANK_LEFT_BTN] = (struct GameButton){ bankLeftRect, { KEY_Q }, 1, true };
	buttons[BANK_RIGHT_BTN] = (struct GameButton){ bankRightRect, { KEY_E }, 1, true };
	buttons[FIRE_PRIMARY_BTN] = (struct GameButton){ firePrimaryRect, { KEY_LCTRL }, 1, false };
	buttons[FIRE_SECONDARY_BTN] = (struct GameButton){ fireSecondaryRect, { KEY_SPACEBAR }, 1, false };
	buttons[TOGGLE_PRIMARY_BTN] = (struct GameButton){ togglePrimaryRect, { KEY_1 }, 1, false };
	buttons[TOGGLE_SECONDARY_BTN] = (struct GameButton){ toggleSecondaryRect, { KEY_6 }, 1, false };
	buttons[FIRE_FLARE_BTN] = (struct GameButton){ fireFlareRect, { KEY_F }, 1, false };
	buttons[ACCELERATE_SLIDE_LEFT_BTN] = (struct GameButton){ accelerateSlideLeftRect, { KEY_A, KEY_PAD1 }, 2, true };
	buttons[ACCELERATE_SLIDE_RIGHT_BTN] = (struct GameButton){ accelerateSlideRightRect, { KEY_A, KEY_PAD3 }, 2, true };
	buttons[REVERSE_SLIDE_LEFT_BTN] = (struct GameButton){ reverseSlideLeftRect, { KEY_Z, KEY_PAD1 }, 2, true };
	buttons[REVERSE_SLIDE_RIGHT_BTN] = (struct GameButton){ reverseSlideRightRect, { KEY_Z, KEY_PAD3 }, 2, true };
	buttons[MENU_BTN] = (struct GameButton){ menuRect, { KEY_ESC }, 1, false };
	buttons[MAP_BTN] = (struct GameButton){ mapRect, { KEY_TAB }, 1, false };
	buttons[TOGGLE_COCKPIT_BTN] = (struct GameButton){ toggleCockpitRect, { KEY_F3 }, 1, false };
	buttons[REAR_VIEW_BTN] = (struct GameButton){ rearViewRect, { KEY_R }, 1, false };
	buttons[HEADLIGHT_BTN] = (struct GameButton){ headlightRect, { KEY_H }, 1, false };
	buttons[ENERGY_TRANSFER_BTN] = (struct GameButton){ energyTransferRect, { KEY_T }, 1, false };
	buttons[GUIDE_BOT_MENU] = (struct GameButton){ guideBotMenuRect, { KEY_LSHIFT, KEY_F4 }, 2, false };
	buttons[PLACE_MARKER] = (struct GameButton){ placeMarkerRect, { KEY_F4 }, 1, false };
	
	// Finish
	return self;
}

- (void)drawRect:(CGRect)rect {
	CGContextRef currentContext = UIGraphicsGetCurrentContext();
	
	// Draw the buttons
	if(Game_mode == GM_NORMAL && !In_screen) {
		CGContextSetRGBFillColor(currentContext, 1.0, 1.0, 1.0, 0.3);
		for(int i = 0; i < 13; ++ i) {
			UIRectFillUsingBlendMode(buttons[i].rect, kCGBlendModeNormal);
		}
		CGContextSetRGBFillColor(currentContext, 1.0, 1.0, 1.0, 0.1);
		for(int i = 13; i < 17; ++ i) {
			UIRectFillUsingBlendMode(buttons[i].rect, kCGBlendModeNormal);
		}
		CGContextSetRGBFillColor(currentContext, 1.0, 1.0, 1.0, 0.3);
		for (int i = 17; i < NUM_BUTTONS; ++i) {
			UIRectFillUsingBlendMode(buttons[i].rect, kCGBlendModeNormal);
		}
	} else {
		trackingTouch = nil;
	}
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
	NSValue *touchValue;
	long accelerateTime;
	
	if(Game_mode == GM_NORMAL && !In_screen) {
		for(UITouch *touch in touches) {
			touchValue = [NSValue valueWithPointer:(__bridge const void * _Nullable)(touch)];
			for(int i = 0; i < NUM_BUTTONS; ++i) {
				if ([self point:[touch locationInView:self] inRect:buttons[i].rect]) {

					// Double-tapping accelerate triggers afterburner
					if (i == ACCELERATE_BTN) {
						accelerateTime = timer_get_usecs();
						if (accelerateTime - accelerateLastTouched < 500000) {
							key_handler(KEY_S, YES);
						}
						accelerateLastTouched = timer_get_usecs();
					}

					[touchButtons removeObjectForKey:touchValue];
					[touchButtons setObject:[NSNumber numberWithInteger:i] forKey:touchValue];
					for(int j = 0; j < buttons[i].nKeys; ++j) {
						key_handler(buttons[i].keys[j], YES);
					}
				}
			}
		}
	} else {
		[super touchesBegan:touches withEvent:event];
	}
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
	NSValue *touchValue;
	NSNumber *buttonNumber;
	int buttonIndex;
	if(Game_mode == GM_NORMAL && !In_screen) {
		for(UITouch *touch in touches) {
			touchValue = [NSValue valueWithPointer:(__bridge const void * _Nullable)(touch)];
			if ([trackingTouch isEqualToValue:touchValue]) {
				trackingTouch = nil;
				touch_dx = touch_dy = 0;
			}
			buttonNumber = [touchButtons objectForKey:touchValue];
			if (buttonNumber == nil) {
				return;
			}
			buttonIndex = [buttonNumber intValue];
			if (buttonIndex == ACCELERATE_BTN) {
				key_handler(KEY_S, NO);
			}
			for(int i = 0; i < buttons[buttonIndex].nKeys; ++ i) {
				key_handler(buttons[buttonIndex].keys[i], NO);
			}
			[touchButtons removeObjectForKey:touchValue];
		}
	} else {
		[super touchesEnded:touches withEvent:event];
	}
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
	int currentButtonIndex, previousButtonIndex;
	int i, j;
	int nSharedKeys = 0;
	unsigned char sharedKeys[MAX_KEYS];
	bool ignoreKey;
	
	NSValue *touchValue;
	NSNumber *prev;
	
	if (Game_mode == GM_NORMAL && !In_screen) {
		for(UITouch *touch in touches) {
			currentButtonIndex = previousButtonIndex = -1;
			touchValue = [NSValue valueWithPointer:(__bridge const void * _Nullable)(touch)];
			
			// Track the touch for ship orientation
			// (go outside the movement pad so the player doesn't accedentally spin the ship)
			if(trackingTouch == nil && [touch locationInView:self].x > buttons[BANK_RIGHT_BTN].rect.origin.x + buttons[BANK_RIGHT_BTN].rect.size.width + 50) {
				trackingTouch = touchValue;
			} else if (trackingTouch == nil) {
				touch_dx = touch_dy = 0;
			}
			if ([trackingTouch isEqualToValue:touchValue]) {
				touch_dx = ([touch locationInView:self].x - [touch previousLocationInView:self].x);
				touch_dy = ([touch previousLocationInView:self].y - [touch locationInView:self].y);
			}
			
			// Get the button the touch is currently in
			for (i = 0; i < NUM_BUTTONS; ++i) {
				if ([self point:[touch locationInView:self] inRect:buttons[i].rect]) {
					currentButtonIndex = i;
					break;
				}
			}
			
			// Find the button the touch was previously in
			prev = [touchButtons objectForKey:touchValue];
			if (prev != nil) {
				previousButtonIndex = [prev intValue];
			}
			
			// Might press button if we hover over it
			if (currentButtonIndex > -1 && previousButtonIndex == -1 && buttons[currentButtonIndex].activateOnHover) {
				[touchButtons removeObjectForKey:touchValue];
				[touchButtons setObject:[NSNumber numberWithInt:i] forKey:touchValue];
				for (i = 0; i < buttons[currentButtonIndex].nKeys; ++i) {
					key_handler(buttons[currentButtonIndex].keys[i], YES);
				}
			}
			
			// We changed buttons. Let the fun begin!
			else if (currentButtonIndex > -1 && previousButtonIndex > -1 && currentButtonIndex != previousButtonIndex) {
				[touchButtons removeObjectForKey:touchValue];
				[touchButtons setObject:[NSNumber numberWithInt:currentButtonIndex] forKey:touchValue];
				
				// Find the keys the two buttons have in common
				for (i = 0; i < buttons[currentButtonIndex].nKeys; ++i) {
					for (j = 0; j < buttons[previousButtonIndex].nKeys; ++j) {
						if (buttons[currentButtonIndex].keys[i] == buttons[previousButtonIndex].keys[j]) {
							sharedKeys[nSharedKeys] = buttons[currentButtonIndex].keys[i];
							++nSharedKeys;
						}
					}
				}
				
				// Press all the keys that are in the new button that weren't in the old one
				for (i = 0; i < buttons[currentButtonIndex].nKeys; ++i) {
					ignoreKey = false;
					for (j = 0; j < nSharedKeys; ++j) {
						if (buttons[currentButtonIndex].keys[i] == sharedKeys[j]) {
							ignoreKey = true;
							break;
						}
					}
					if (!ignoreKey) {
						key_handler(buttons[currentButtonIndex].keys[i], YES);
					}
				}
				
				// Release all the keys that were in the old button that aren't in the new one
				for (i = 0; i < buttons[previousButtonIndex].nKeys; ++i) {
					ignoreKey = false;
					for (j = 0; j < nSharedKeys; ++j) {
						if (buttons[previousButtonIndex].keys[i] == sharedKeys[j]) {
							ignoreKey = true;
							break;
						}
					}
					if (!ignoreKey) {
						key_handler(buttons[previousButtonIndex].keys[i], NO);
					}
				}
			}
		}
	} else {
		[super touchesMoved:touches withEvent:event];
	}
}

@end
