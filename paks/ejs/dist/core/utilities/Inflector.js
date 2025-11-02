/**
 * Inflector - String inflection utilities
 *
 * Provides string manipulation for pluralization, camelization, etc.
 * @spec ejs
 * @stability evolving
 */
export class Inflector {
    static irregulars = {
        'child': 'children',
        'person': 'people',
        'man': 'men',
        'woman': 'women',
        'tooth': 'teeth',
        'foot': 'feet',
        'mouse': 'mice',
        'goose': 'geese'
    };
    /**
     * Convert string to plural form
     * @param word Word to pluralize
     * @returns Pluralized word
     */
    static pluralize(word) {
        // Check irregulars
        const lower = word.toLowerCase();
        if (lower in Inflector.irregulars) {
            return Inflector.irregulars[lower];
        }
        // Apply rules
        if (word.endsWith('s') || word.endsWith('x') || word.endsWith('z') ||
            word.endsWith('ch') || word.endsWith('sh')) {
            return word + 'es';
        }
        if (word.endsWith('y') && !/[aeiou]y$/.test(word)) {
            return word.slice(0, -1) + 'ies';
        }
        if (word.endsWith('f')) {
            return word.slice(0, -1) + 'ves';
        }
        if (word.endsWith('fe')) {
            return word.slice(0, -2) + 'ves';
        }
        return word + 's';
    }
    /**
     * Convert string to singular form
     * @param word Word to singularize
     * @returns Singularized word
     */
    static singularize(word) {
        // Check reverse irregulars
        for (const [singular, plural] of Object.entries(Inflector.irregulars)) {
            if (word.toLowerCase() === plural) {
                return singular;
            }
        }
        // Apply rules (reverse of pluralize)
        if (word.endsWith('ies')) {
            return word.slice(0, -3) + 'y';
        }
        if (word.endsWith('ves')) {
            return word.slice(0, -3) + 'f';
        }
        if (word.endsWith('es')) {
            return word.slice(0, -2);
        }
        if (word.endsWith('s')) {
            return word.slice(0, -1);
        }
        return word;
    }
    /**
     * Convert string to camel case
     * @param str String to camelize
     * @returns Camelized string
     */
    static camelize(str) {
        return str
            .replace(/[_-](.)/g, (_, c) => c.toUpperCase())
            .replace(/^(.)/, (_, c) => c.toLowerCase());
    }
    /**
     * Convert string to pascal case
     * @param str String to pascalize
     * @returns Pascalized string
     */
    static pascalize(str) {
        const camel = Inflector.camelize(str);
        return camel.charAt(0).toUpperCase() + camel.slice(1);
    }
    /**
     * Convert string to snake case
     * @param str String to convert
     * @returns Snake case string
     */
    static snakeCase(str) {
        return str
            .replace(/([A-Z])/g, '_$1')
            .toLowerCase()
            .replace(/^_/, '');
    }
    /**
     * Convert string to kebab case
     * @param str String to convert
     * @returns Kebab case string
     */
    static kebabCase(str) {
        return str
            .replace(/([A-Z])/g, '-$1')
            .toLowerCase()
            .replace(/^-/, '');
    }
    /**
     * Convert string to title case
     * @param str String to convert
     * @returns Title case string
     */
    static titleize(str) {
        return str
            .split(/[\s_-]+/)
            .map(word => word.charAt(0).toUpperCase() + word.slice(1).toLowerCase())
            .join(' ');
    }
    /**
     * Humanize a string (convert from code format to readable)
     * @param str String to humanize
     * @returns Humanized string
     */
    static humanize(str) {
        return str
            .replace(/[_-]/g, ' ')
            .replace(/([A-Z])/g, ' $1')
            .trim()
            .split(/\s+/)
            .map((word, i) => i === 0
            ? word.charAt(0).toUpperCase() + word.slice(1).toLowerCase()
            : word.toLowerCase())
            .join(' ');
    }
}
//# sourceMappingURL=Inflector.js.map