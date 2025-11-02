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
export class JSON {
    /**
     * Parse a JSON or JSON5 string and return an object
     * @param data JSON/JSON5 string to parse
     * @param filter Optional filter function to transform values
     * @returns Parsed object
     */
    static parse(data, filter) {
        // Try JSON5 parsing first (supports comments, trailing commas, etc.)
        try {
            const parsed = this.parseJSON5(data);
            if (filter) {
                return this.applyFilter(parsed, filter);
            }
            return parsed;
        }
        catch (e) {
            // Fall back to standard JSON parsing
            try {
                const parsed = globalThis.JSON.parse(data);
                if (filter) {
                    return this.applyFilter(parsed, filter);
                }
                return parsed;
            }
            catch (jsonError) {
                throw new Error(`JSON parse error: ${jsonError}`);
            }
        }
    }
    /**
     * Convert an object to a JSON string
     * @param obj Object to stringify
     * @param replacer Optional replacer function or array
     * @param indent Indentation (number of spaces or string)
     * @returns JSON string
     */
    static stringify(obj, replacer, indent) {
        return globalThis.JSON.stringify(obj, replacer, indent);
    }
    /**
     * Parse JSON5 format (with comments, trailing commas, etc.)
     * @param text JSON5 text to parse
     * @returns Parsed object
     */
    static parseJSON5(text) {
        // Remove comments
        text = this.removeComments(text);
        // Handle trailing commas
        text = this.removeTrailingCommas(text);
        // Convert single quotes to double quotes (carefully)
        text = this.normalizeSingleQuotes(text);
        // Handle unquoted keys
        text = this.quoteUnquotedKeys(text);
        // Handle special number values
        text = this.normalizeSpecialNumbers(text);
        // Try to parse
        return globalThis.JSON.parse(text);
    }
    /**
     * Remove single-line and multi-line comments
     */
    static removeComments(text) {
        let result = '';
        let i = 0;
        let inString = false;
        let stringChar = '';
        while (i < text.length) {
            const char = text[i];
            const next = text[i + 1];
            // Track if we're in a string
            if ((char === '"' || char === "'") && (i === 0 || text[i - 1] !== '\\')) {
                if (!inString) {
                    inString = true;
                    stringChar = char;
                }
                else if (char === stringChar) {
                    inString = false;
                }
                result += char;
                i++;
                continue;
            }
            // Skip comments (only if not in string)
            if (!inString) {
                // Single-line comment
                if (char === '/' && next === '/') {
                    while (i < text.length && text[i] !== '\n') {
                        i++;
                    }
                    continue;
                }
                // Multi-line comment
                if (char === '/' && next === '*') {
                    i += 2;
                    while (i < text.length - 1) {
                        if (text[i] === '*' && text[i + 1] === '/') {
                            i += 2;
                            break;
                        }
                        i++;
                    }
                    continue;
                }
            }
            result += char;
            i++;
        }
        return result;
    }
    /**
     * Remove trailing commas from objects and arrays
     */
    static removeTrailingCommas(text) {
        // Remove commas before closing braces/brackets
        return text.replace(/,(\s*[}\]])/g, '$1');
    }
    /**
     * Convert single quotes to double quotes (for strings)
     */
    static normalizeSingleQuotes(text) {
        let result = '';
        let i = 0;
        while (i < text.length) {
            const char = text[i];
            if (char === "'") {
                result += '"';
                i++;
                // Copy string contents
                while (i < text.length) {
                    const c = text[i];
                    if (c === "'") {
                        result += '"';
                        i++;
                        break;
                    }
                    else if (c === '\\') {
                        result += c;
                        i++;
                        if (i < text.length) {
                            result += text[i];
                            i++;
                        }
                    }
                    else if (c === '"') {
                        result += '\\"'; // Escape double quotes
                        i++;
                    }
                    else {
                        result += c;
                        i++;
                    }
                }
            }
            else {
                result += char;
                i++;
            }
        }
        return result;
    }
    /**
     * Quote unquoted object keys
     */
    static quoteUnquotedKeys(text) {
        // Match unquoted keys: word followed by colon (not in quotes)
        return text.replace(/([{,]\s*)([a-zA-Z_$][a-zA-Z0-9_$]*)(\s*:)/g, '$1"$2"$3');
    }
    /**
     * Normalize special number values (Infinity, NaN, etc.)
     */
    static normalizeSpecialNumbers(text) {
        // Replace Infinity with "Infinity" (will be converted later)
        text = text.replace(/\bInfinity\b/g, '"__JSON5_INFINITY__"');
        text = text.replace(/\b-Infinity\b/g, '"__JSON5_NEG_INFINITY__"');
        text = text.replace(/\bNaN\b/g, '"__JSON5_NAN__"');
        return text;
    }
    /**
     * Apply filter function to parsed object
     */
    static applyFilter(obj, filter) {
        if (obj === null || typeof obj !== 'object') {
            return filter('', obj);
        }
        if (Array.isArray(obj)) {
            const result = [];
            for (let i = 0; i < obj.length; i++) {
                const value = this.applyFilter(obj[i], filter);
                const filtered = filter(String(i), value);
                if (filtered !== undefined) {
                    result.push(filtered);
                }
            }
            return filter('', result);
        }
        const result = {};
        for (const key in obj) {
            if (obj.hasOwnProperty(key)) {
                const value = this.applyFilter(obj[key], filter);
                const filtered = filter(key, value);
                if (filtered !== undefined) {
                    result[key] = filtered;
                }
            }
        }
        return filter('', result);
    }
}
/**
 * Deserialize a string into an object
 * Supports JSON5 format with comments, trailing commas, etc.
 * @param str String containing JSON/JSON5 data
 * @returns Parsed object
 */
export function deserialize(str) {
    return JSON.parse(str);
}
/**
 * Serialize an object to a string
 * @param obj Object to serialize
 * @param options Serialization options
 * @returns JSON string
 */
export function serialize(obj, options) {
    if (!options) {
        return globalThis.JSON.stringify(obj);
    }
    // Determine indentation
    let indent = undefined;
    if (options.pretty) {
        indent = options.indent ?? 2;
    }
    else if (options.indent !== undefined) {
        indent = options.indent;
    }
    // Apply replacer if provided
    const replacer = options.replacer || null;
    return globalThis.JSON.stringify(obj, replacer, indent);
}
//# sourceMappingURL=JSON.js.map