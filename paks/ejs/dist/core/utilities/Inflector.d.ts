/**
 * Inflector - String inflection utilities
 *
 * Provides string manipulation for pluralization, camelization, etc.
 * @spec ejs
 * @stability evolving
 */
export declare class Inflector {
    private static irregulars;
    /**
     * Convert string to plural form
     * @param word Word to pluralize
     * @returns Pluralized word
     */
    static pluralize(word: string): string;
    /**
     * Convert string to singular form
     * @param word Word to singularize
     * @returns Singularized word
     */
    static singularize(word: string): string;
    /**
     * Convert string to camel case
     * @param str String to camelize
     * @returns Camelized string
     */
    static camelize(str: string): string;
    /**
     * Convert string to pascal case
     * @param str String to pascalize
     * @returns Pascalized string
     */
    static pascalize(str: string): string;
    /**
     * Convert string to snake case
     * @param str String to convert
     * @returns Snake case string
     */
    static snakeCase(str: string): string;
    /**
     * Convert string to kebab case
     * @param str String to convert
     * @returns Kebab case string
     */
    static kebabCase(str: string): string;
    /**
     * Convert string to title case
     * @param str String to convert
     * @returns Title case string
     */
    static titleize(str: string): string;
    /**
     * Humanize a string (convert from code format to readable)
     * @param str String to humanize
     * @returns Humanized string
     */
    static humanize(str: string): string;
}
//# sourceMappingURL=Inflector.d.ts.map