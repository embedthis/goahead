/**
 * Number extensions
 *
 * Adds Ejscript methods to the Number type
 * @spec ejs
 */
/**
 * Format number as string
 */
Number.prototype.format = function (options) {
    let num = this.valueOf();
    let result = num.toString();
    if (options?.decimals !== undefined) {
        result = num.toFixed(options.decimals);
    }
    if (options?.thousands) {
        const parts = result.split('.');
        parts[0] = parts[0].replace(/\B(?=(\d{3})+(?!\d))/g, ',');
        result = parts.join('.');
    }
    return result;
};
/**
 * Maximum 32-bit integer
 */
Object.defineProperty(Number, 'MaxInt32', {
    value: 2147483647,
    writable: false,
    enumerable: false,
    configurable: false
});
export {};
//# sourceMappingURL=NumberExtensions.js.map