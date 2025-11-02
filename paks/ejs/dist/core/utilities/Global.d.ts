/**
 * Global utility functions
 *
 * Provides global helper functions like blend, print, etc.
 * These functions match the Ejscript Global.es API
 * @spec ejs
 */
/**
 * Blend objects together
 * Merges source object properties into destination
 * @param dest Destination object
 * @param src Source object
 * @param options Blend options
 * @returns Destination object
 */
export declare function blend(dest: any, src: any, options?: {
    overwrite?: boolean;
    functions?: boolean;
}): any;
/**
 * Clone an object
 * @param obj Object to clone
 * @param deep Perform deep clone
 * @returns Cloned object
 */
export declare function clone(obj: any, deep?: boolean): any;
/**
 * Format a string with template substitution
 * @param template Template string with ${var} placeholders
 * @param vars Variables object
 * @returns Formatted string
 */
export declare function format(template: string, vars: Record<string, any>): string;
/**
 * Hash a string to MD5
 * @param str String to hash
 * @returns MD5 hash
 */
export declare function md5(str: string): string;
/**
 * Hash a string to SHA256
 * @param str String to hash
 * @returns SHA256 hash
 */
export declare function sha256(str: string): string;
/**
 * Dump objects for debugging (variadic version matching ejscript)
 * Serializes objects and prints to standard output
 * @param args Objects to dump
 */
export declare function dump(...args: any[]): void;
/**
 * Dump all object properties including hidden ones
 * @param args Objects to dump
 */
export declare function dumpAll(...args: any[]): void;
/**
 * Dump object property descriptors
 * @param args Objects to dump
 */
export declare function dumpDef(...args: any[]): void;
/**
 * Print arguments to standard output with newline
 * @param args Values to print
 */
export declare function print(...args: any[]): void;
/**
 * Print arguments to standard output without newline
 * @param args Values to print
 */
export declare function prints(...args: any[]): void;
/**
 * Print formatted output using printf-style formatting
 * Uses String.format() for formatting
 * @param fmt Format string
 * @param args Arguments to format
 */
export declare function printf(fmt: string, ...args: any[]): void;
/**
 * Print hash information for debugging
 * @param name Label for the object
 * @param obj Object to print hash for
 */
export declare function printHash(name: string, obj: any): void;
/**
 * Get unique hash code for an object
 * @param obj Object to hash
 * @returns Unique hash identifier
 */
export declare function hashcode(obj: any): number;
/**
 * Assert that a condition is true, throw error if false
 * @param condition Condition to test
 * @param message Optional error message
 */
export declare function assert(condition: boolean, message?: string): void;
/**
 * Encode a string to base64
 * @param str String to encode
 * @returns Base64 encoded string
 */
export declare function base64(str: string): string;
/**
 * Decode a base64 string
 * @param str Base64 string to decode
 * @returns Decoded string
 */
export declare function base64Decode(str: string): string;
/**
 * Parse a string and convert to a primitive type
 * Attempts to intelligently parse strings into appropriate types
 * @param str String to parse
 * @param preferredType Preferred type to use
 * @returns Parsed value
 */
export declare function parse(str: string, preferredType?: any): any;
/**
 * Parse a string as a floating point number
 * @param str String to parse
 * @returns Parsed number
 */
export declare function parseFloat(str: string): number;
/**
 * Parse a string as an integer with specified radix
 * @param str String to parse
 * @param radix Base for parsing (default 10)
 * @returns Parsed integer
 */
export declare function parseInt(str: string, radix?: number): number;
/**
 * Check if value is NaN
 * @param value Value to check
 * @returns True if NaN
 */
export declare function isNaN(value: number): boolean;
/**
 * Check if value is finite
 * @param value Value to check
 * @returns True if finite
 */
export declare function isFinite(value: number): boolean;
/**
 * Check if object is instance of a type
 * @param obj Object to check
 * @param target Target type
 * @returns True if obj is instance of target
 */
export declare function instanceOf(obj: any, target: any): boolean;
/**
 * Evaluate a script string (uses eval)
 * WARNING: eval is dangerous and should be used carefully
 * @param script Script string to evaluate
 * @returns Result of evaluation
 */
export declare function evalScript(script: string): any;
/**
 * Load a script file (simplified version)
 * In Bun, use import() instead
 * @param file File path to load
 * @returns Loaded module
 */
export declare function load(file: string): Promise<any>;
import { Timer } from './Timer';
/**
 * Create an interval timer that repeats
 * @param callback Function to invoke repeatedly
 * @param delay Time period in milliseconds between invocations
 * @param args Arguments to pass to callback
 * @returns Timer instance
 */
export declare function setIntervalTimer(callback: Function, delay: number, ...args: any[]): Timer;
/**
 * Clear and dispose of an interval timer
 * @param timer Timer returned from setIntervalTimer
 */
export declare function clearIntervalTimer(timer: Timer): void;
/**
 * Create a one-shot timeout timer
 * @param callback Function to invoke when timer expires
 * @param delay Time in milliseconds until timer expires
 * @param args Arguments to pass to callback
 * @returns Timer instance
 */
export declare function setTimeoutTimer(callback: Function, delay: number, ...args: any[]): Timer;
/**
 * Clear and dispose of a timeout timer
 * @param timer Timer returned from setTimeoutTimer
 */
export declare function clearTimeoutTimer(timer: Timer): void;
//# sourceMappingURL=Global.d.ts.map