/**
 * Number extensions
 *
 * Adds Ejscript methods to the Number type
 * @spec ejs
 */
declare global {
    interface Number {
        format(options?: {
            decimals?: number;
            thousands?: boolean;
        }): string;
    }
    interface NumberConstructor {
        readonly MaxInt32: number;
    }
}
export {};
//# sourceMappingURL=NumberExtensions.d.ts.map