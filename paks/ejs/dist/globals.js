/**
 * Global namespace and variables for Ejscript compatibility
 */
/**
 * Product singleton object
 * @spec ejs
 */
export const ejs = {};
/**
 * Standard output text stream
 * @spec ejs
 */
export let stdout;
/**
 * Standard input text stream
 * @spec ejs
 */
export let stdin;
/**
 * Standard error text stream
 * @spec ejs
 */
export let stderr;
/**
 * Initialize global streams once App is initialized
 * This is called internally by App during initialization
 * @internal
 */
export function initGlobalStreams() {
    // These will be set by App initialization
    // stdout = App.stdout
    // stdin = App.stdin
    // stderr = App.stderr
}
/**
 * Constant set to true in all Ejscript interpreters
 * @spec ejs
 */
export const EJSCRIPT = true;
/**
 * Global print function
 * Writes output to stdout
 */
export function print(...args) {
    console.log(...args);
}
/**
 * Load a module dynamically
 * @param module Module path to load
 * @param options Load options
 */
export async function load(module, options) {
    // Use dynamic import
    return import(module);
}
//# sourceMappingURL=globals.js.map