/**
 * Date extensions
 *
 * Adds Ejscript methods to the Date prototype
 * @spec ejs
 */
declare global {
    interface Date {
        elapsed: number;
        format(fmt?: string): string;
        future(msec: number): Date;
        toUTCString(): string;
    }
    interface DateConstructor {
        parseUTCDate(str: string): Date;
    }
}
export {};
//# sourceMappingURL=DateExtensions.d.ts.map