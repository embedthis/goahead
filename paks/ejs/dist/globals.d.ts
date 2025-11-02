/**
 * Global namespace and variables for Ejscript compatibility
 */
import type { TextStream } from './core/streams/TextStream';
/**
 * Product singleton object
 * @spec ejs
 */
export declare const ejs: Record<string, any>;
/**
 * Standard output text stream
 * @spec ejs
 */
export declare let stdout: TextStream;
/**
 * Standard input text stream
 * @spec ejs
 */
export declare let stdin: TextStream;
/**
 * Standard error text stream
 * @spec ejs
 */
export declare let stderr: TextStream;
/**
 * Initialize global streams once App is initialized
 * This is called internally by App during initialization
 * @internal
 */
export declare function initGlobalStreams(): void;
/**
 * Constant set to true in all Ejscript interpreters
 * @spec ejs
 */
export declare const EJSCRIPT: boolean;
/**
 * Global print function
 * Writes output to stdout
 */
export declare function print(...args: any[]): void;
/**
 * Load a module dynamically
 * @param module Module path to load
 * @param options Load options
 */
export declare function load(module: string, options?: {
    reload?: boolean;
}): Promise<any>;
//# sourceMappingURL=globals.d.ts.map