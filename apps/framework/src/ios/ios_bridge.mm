#import <Foundation/Foundation.h>
#include "calculator.h" // Assuming calculator.h is accessible in include path

// Define a C-style function wrapper that can be easily called from Swift/Objective-C
// or a simple Objective-C class wrapper.
// For this example, let's use a C-style function for simplicity to avoid creating a class interface.

extern "C" {
    int calculate_add(int a, int b) {
        Calculator calculator;
        return calculator.add(a, b);
    }
}

// Alternatively, an Objective-C class wrapper could look like this:
/*
@interface MyFrameworkBridge : NSObject
- (int)add:(int)a withB:(int)b;
@end

@implementation MyFrameworkBridge
- (int)add:(int)a withB:(int)b {
    Calculator calculator;
    return calculator.add(a, b);
}
@end
*/
