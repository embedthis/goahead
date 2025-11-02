/**
 * Array extensions
 *
 * Adds Ejscript methods to the Array prototype
 * @spec ejs
 */
declare global {
    interface Array<T> {
        contains(item: T): boolean;
        append(item: T | T[]): this;
        clear(): void;
        compact(): this;
        remove(start: number, end?: number): void;
        transform(fn: (item: T, index: number, array: T[]) => any): this;
        clone(deep?: boolean): T[];
        unique(): T[];
        findAll(match: (item: T, index: number, array: T[]) => boolean): T[];
        reject(match: (item: T, index: number, array: T[]) => boolean): T[];
    }
}
export {};
//# sourceMappingURL=ArrayExtensions.d.ts.map