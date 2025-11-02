/**
 * String extensions
 *
 * Adds Ejscript methods to the String prototype
 * @spec ejs
 */
declare global {
    interface String {
        caseCompare(compare: string): number;
        caselessCompare(compare: string): number;
        contains(pattern: string | RegExp): boolean;
        startsWith(prefix: string): boolean;
        endsWith(suffix: string): boolean;
        isDigit: boolean;
        isAlpha: boolean;
        isAlphaNum: boolean;
        isLower: boolean;
        isSpace: boolean;
        isUpper: boolean;
        format(...args: any[]): string;
        expand(vars: Record<string, any>, options?: {
            fill?: any;
            join?: string;
        }): string;
        printable(): string;
        quote(): string;
        remove(start: number, end?: number): string;
        reverse(): string;
        times(count: number): string;
        tokenize(format: string): string[];
        toPascal(): string;
        toCamel(): string;
        capitalize(): string;
        trim(str?: string | null): string;
        trimStart(str?: string | null): string;
        trimEnd(str?: string | null): string;
        parseJSON(filter?: Function | null): any;
        toPath(): any;
        toJSON(): string;
    }
}
export {};
//# sourceMappingURL=StringExtensions.d.ts.map