/**
 * Global utility functions
 *
 * Provides global helper functions like blend, print, etc.
 * These functions match the Ejscript Global.es API
 * @spec ejs
 */
import { App } from '../App';
import { serialize } from '../JSON';
/**
 * Blend objects together
 * Merges source object properties into destination
 * @param dest Destination object
 * @param src Source object
 * @param options Blend options
 * @returns Destination object
 */
export function blend(dest, src, options) {
    const overwrite = options?.overwrite !== false;
    const includeFunctions = options?.functions !== false;
    for (const key in src) {
        if (!src.hasOwnProperty(key))
            continue;
        const value = src[key];
        // Skip functions unless allowed
        if (typeof value === 'function' && !includeFunctions) {
            continue;
        }
        if (overwrite || !(key in dest)) {
            if (value && typeof value === 'object' && !Array.isArray(value) && value.constructor === Object) {
                // Recursively blend plain objects
                if (typeof dest[key] !== 'object' || dest[key] === null) {
                    dest[key] = {};
                }
                blend(dest[key], value, options);
            }
            else {
                dest[key] = value;
            }
        }
    }
    return dest;
}
// Note: serialize() and deserialize() have been moved to JSON.ts
// They are re-exported from index.ts for convenience
/**
 * Clone an object
 * @param obj Object to clone
 * @param deep Perform deep clone
 * @returns Cloned object
 */
export function clone(obj, deep = true) {
    if (obj === null || typeof obj !== 'object') {
        return obj;
    }
    if (Array.isArray(obj)) {
        return deep ? obj.map(item => clone(item, true)) : [...obj];
    }
    if (obj instanceof Date) {
        return new Date(obj);
    }
    if (obj instanceof RegExp) {
        return new RegExp(obj);
    }
    if (deep) {
        const cloned = {};
        for (const key in obj) {
            if (obj.hasOwnProperty(key)) {
                cloned[key] = clone(obj[key], true);
            }
        }
        return cloned;
    }
    else {
        return { ...obj };
    }
}
/**
 * Format a string with template substitution
 * @param template Template string with ${var} placeholders
 * @param vars Variables object
 * @returns Formatted string
 */
export function format(template, vars) {
    return template.replace(/\$\{([^}]+)\}/g, (match, key) => {
        return key in vars ? String(vars[key]) : match;
    });
}
/**
 * Hash a string to MD5
 * @param str String to hash
 * @returns MD5 hash
 */
export function md5(str) {
    // Use Bun's built-in crypto
    const hasher = new Bun.CryptoHasher('md5');
    hasher.update(str);
    return hasher.digest('hex');
}
/**
 * Hash a string to SHA256
 * @param str String to hash
 * @returns SHA256 hash
 */
export function sha256(str) {
    const hasher = new Bun.CryptoHasher('sha256');
    hasher.update(str);
    return hasher.digest('hex');
}
/**
 * Dump objects for debugging (variadic version matching ejscript)
 * Serializes objects and prints to standard output
 * @param args Objects to dump
 */
export function dump(...args) {
    for (const obj of args) {
        print(serialize(obj, { pretty: true }));
    }
}
/**
 * Dump all object properties including hidden ones
 * @param args Objects to dump
 */
export function dumpAll(...args) {
    for (const obj of args) {
        print(serialize(obj, { pretty: true }));
    }
}
/**
 * Dump object property descriptors
 * @param args Objects to dump
 */
export function dumpDef(...args) {
    for (const obj of args) {
        const names = Object.getOwnPropertyNames(obj);
        for (const key of names) {
            const descriptor = Object.getOwnPropertyDescriptor(obj, key);
            print(`${key}: ${serialize(descriptor, { pretty: true })}`);
        }
    }
}
/**
 * Print arguments to standard output with newline
 * @param args Values to print
 */
export function print(...args) {
    const output = args.map(arg => String(arg)).join(' ');
    App.outputStream.write(output + '\n');
}
/**
 * Print arguments to standard output without newline
 * @param args Values to print
 */
export function prints(...args) {
    const output = args.map(arg => String(arg)).join(' ');
    App.outputStream.write(output);
}
/**
 * Print formatted output using printf-style formatting
 * Uses String.format() for formatting
 * @param fmt Format string
 * @param args Arguments to format
 */
export function printf(fmt, ...args) {
    // Use the String.format() extension method
    const formatted = fmt.format(args);
    App.outputStream.write(formatted);
}
/**
 * Print hash information for debugging
 * @param name Label for the object
 * @param obj Object to print hash for
 */
export function printHash(name, obj) {
    print(`${name.padStart(20)} ${hashcode(obj).toString(16).toUpperCase()}`);
}
/**
 * Get unique hash code for an object
 * @param obj Object to hash
 * @returns Unique hash identifier
 */
export function hashcode(obj) {
    // Simple hash implementation
    if (obj === null || obj === undefined) {
        return 0;
    }
    if (typeof obj === 'number') {
        return obj | 0;
    }
    if (typeof obj === 'boolean') {
        return obj ? 1 : 0;
    }
    if (typeof obj === 'string') {
        let hash = 0;
        for (let i = 0; i < obj.length; i++) {
            const char = obj.charCodeAt(i);
            hash = ((hash << 5) - hash) + char;
            hash = hash & hash; // Convert to 32-bit integer
        }
        return hash;
    }
    // For objects, use a weak map to store consistent hash codes
    if (!hashCodeMap.has(obj)) {
        hashCodeMap.set(obj, hashCodeCounter++);
    }
    return hashCodeMap.get(obj);
}
// WeakMap to store object hash codes
const hashCodeMap = new WeakMap();
let hashCodeCounter = 1;
/**
 * Assert that a condition is true, throw error if false
 * @param condition Condition to test
 * @param message Optional error message
 */
export function assert(condition, message) {
    if (!condition) {
        throw new Error(message || 'Assertion failed');
    }
}
/**
 * Encode a string to base64
 * @param str String to encode
 * @returns Base64 encoded string
 */
export function base64(str) {
    return Buffer.from(str, 'utf-8').toString('base64');
}
/**
 * Decode a base64 string
 * @param str Base64 string to decode
 * @returns Decoded string
 */
export function base64Decode(str) {
    return Buffer.from(str, 'base64').toString('utf-8');
}
/**
 * Parse a string and convert to a primitive type
 * Attempts to intelligently parse strings into appropriate types
 * @param str String to parse
 * @param preferredType Preferred type to use
 * @returns Parsed value
 */
export function parse(str, preferredType) {
    if (str === 'true')
        return true;
    if (str === 'false')
        return false;
    if (str === 'null')
        return null;
    if (str === 'undefined')
        return undefined;
    // Try number
    if (preferredType === Number || /^-?\d+\.?\d*$/.test(str)) {
        const num = Number(str);
        if (!isNaN(num))
            return num;
    }
    // Try JSON
    try {
        return JSON.parse(str);
    }
    catch {
        // Return as string if all else fails
        return str;
    }
}
/**
 * Parse a string as a floating point number
 * @param str String to parse
 * @returns Parsed number
 */
export function parseFloat(str) {
    return Number.parseFloat(str);
}
/**
 * Parse a string as an integer with specified radix
 * @param str String to parse
 * @param radix Base for parsing (default 10)
 * @returns Parsed integer
 */
export function parseInt(str, radix = 10) {
    return Number.parseInt(str, radix);
}
/**
 * Check if value is NaN
 * @param value Value to check
 * @returns True if NaN
 */
export function isNaN(value) {
    return Number.isNaN(value);
}
/**
 * Check if value is finite
 * @param value Value to check
 * @returns True if finite
 */
export function isFinite(value) {
    return Number.isFinite(value);
}
/**
 * Check if object is instance of a type
 * @param obj Object to check
 * @param target Target type
 * @returns True if obj is instance of target
 */
export function instanceOf(obj, target) {
    return obj instanceof target;
}
/**
 * Evaluate a script string (uses eval)
 * WARNING: eval is dangerous and should be used carefully
 * @param script Script string to evaluate
 * @returns Result of evaluation
 */
export function evalScript(script) {
    // eslint-disable-next-line no-eval
    return eval(script);
}
/**
 * Load a script file (simplified version)
 * In Bun, use import() instead
 * @param file File path to load
 * @returns Loaded module
 */
export async function load(file) {
    return import(file);
}
// Timer utility functions (matching ejscript global API)
import { Timer } from './Timer';
/**
 * Create an interval timer that repeats
 * @param callback Function to invoke repeatedly
 * @param delay Time period in milliseconds between invocations
 * @param args Arguments to pass to callback
 * @returns Timer instance
 */
export function setIntervalTimer(callback, delay, ...args) {
    const timer = new Timer(delay, callback, ...args);
    timer.repeat = true;
    timer.start();
    return timer;
}
/**
 * Clear and dispose of an interval timer
 * @param timer Timer returned from setIntervalTimer
 */
export function clearIntervalTimer(timer) {
    timer.stop();
}
/**
 * Create a one-shot timeout timer
 * @param callback Function to invoke when timer expires
 * @param delay Time in milliseconds until timer expires
 * @param args Arguments to pass to callback
 * @returns Timer instance
 */
export function setTimeoutTimer(callback, delay, ...args) {
    const timer = new Timer(delay, callback, ...args);
    timer.start();
    return timer;
}
/**
 * Clear and dispose of a timeout timer
 * @param timer Timer returned from setTimeoutTimer
 */
export function clearTimeoutTimer(timer) {
    timer.stop();
}
//# sourceMappingURL=Global.js.map