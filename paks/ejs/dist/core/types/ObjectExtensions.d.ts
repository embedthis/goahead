/**
 * Object extensions
 *
 * Adds Ejscript utility methods for objects
 * @spec ejs
 */
/**
 * Blend (merge) objects
 */
export declare function blend(dest: any, src: any, options?: {
    overwrite?: boolean;
    functions?: boolean;
}): any;
/**
 * Clone an object
 */
export declare function clone(obj: any, deep?: boolean): any;
/**
 * Get object type
 */
export declare function getType(obj: any): Function;
/**
 * Get object name
 */
export declare function getName(obj: any): string;
declare global {
    interface ObjectConstructor {
        blend(dest: any, src: any, options?: {
            overwrite?: boolean;
            functions?: boolean;
        }): any;
        clone(obj: any, deep?: boolean): any;
        getType(obj: any): Function;
        getName(obj: any): string;
    }
}
//# sourceMappingURL=ObjectExtensions.d.ts.map