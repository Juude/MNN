#include <iostream>
#include "calculator.h" // Assuming calculator.h is accessible from include path

// Basic assertion function for demonstration
void assertEquals(int expected, int actual, const std::string& testName) {
    if (expected == actual) {
        std::cout << "[PASSED] " << testName << std::endl;
    } else {
        std::cout << "[FAILED] " << testName << " Expected: " << expected << " Actual: " << actual << std::endl;
    }
}

int main() {
    Calculator calc;

    // Test case 1: Positive numbers
    assertEquals(5, calc.add(2, 3), "PositiveNumbers");

    // Test case 2: Negative numbers
    assertEquals(-5, calc.add(-2, -3), "NegativeNumbers");

    // Test case 3: Mixed numbers
    assertEquals(1, calc.add(-2, 3), "MixedNumbers");

    // Test case 4: Zero
    assertEquals(0, calc.add(0, 0), "Zero");
    assertEquals(2, calc.add(2, 0), "ZeroAndPositive");
    assertEquals(-2, calc.add(-2, 0), "ZeroAndNegative");

    return 0; // Indicate success if all tests passed (or handle errors for CI)
}
