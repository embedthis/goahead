/**
 * JSON - JSON and JSON5 parsing and serialization
 *
 * Provides encoding and decoding to JavaScript Object Notation strings (JSON5).
 * Supports both standard JSON and JSON5 (with comments, trailing commas, etc.)
 * @spec ejs
 * @stability evolving
 */
/**
 * JSON class providing static methods for parsing and stringifying
 * This implementation supports JSON5 format:
 * - Comments (single-line and multi-line)
 * - Trailing commas in objects and arrays
 * - Single-quoted strings
 * - Unquoted object keys (identifiers)
 * - Hexadecimal numbers
 * - Leading/trailing decimal points
 * - Infinity, -Infinity, NaN
 * - Multi-line strings
 */
export declare class JSON {
    /**
     * Parse a JSON or JSON5 string and return an object
     * @param data JSON/JSON5 string to parse
     * @param filter Optional filter function to transform values
     * @returns Parsed object
     */
    static parse(data: string, filter?: (key: string, value: any) => any): any;
    /**
     * Convert an object to a JSON string
     * @param obj Object to stringify
     * @param replacer Optional replacer function or array
     * @param indent Indentation (number of spaces or string)
     * @returns JSON string
     */
    static stringify(obj: any, replacer?: any, indent?: number | string): string;
    /**
     * Parse JSON5 format (with comments, trailing commas, etc.)
     * @param text JSON5 text to parse
     * @returns Parsed object
     */
    private static parseJSON5;
    /**
     * Remove single-line and multi-line comments
     */
    private static removeComments;
    /**
     * Remove trailing commas from objects and arrays
     */
    private static removeTrailingCommas;
    /**
     * Convert single quotes to double quotes (for strings)
     */
    private static normalizeSingleQuotes;
    /**
     * Quote unquoted object keys
     */
    private static quoteUnquotedKeys;
    /**
     * Normalize special number values (Infinity, NaN, etc.)
     */
    private static normalizeSpecialNumbers;
    /**
     * Apply filter function to parsed object
     */
    private static applyFilter;
}
/**
 * Deserialize a string into an object
 * Supports JSON5 format with comments, trailing commas, etc.
 * @param str String containing JSON/JSON5 data
 * @returns Parsed object
 */
export declare function deserialize(str: string): any;
/**
 * Serialize an object to a string
 * @param obj Object to serialize
 * @param options Serialization options
 * @returns JSON string
 */
export declare function serialize(obj: any, options?: {
    pretty?: boolean;
    indent?: number | string;
    depth?: number;
    hidden?: boolean;
    baseClasses?: boolean;
    commas?: boolean;
    quotes?: boolean;
    multiline?: boolean;
    replacer?: (key: string, value: any) => any;
}): string;
//# sourceMappingURL=JSON.d.ts.map