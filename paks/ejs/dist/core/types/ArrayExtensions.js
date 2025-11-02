/**
 * Array extensions
 *
 * Adds Ejscript methods to the Array prototype
 * @spec ejs
 */
// ============================================================================
// Search Methods
// ============================================================================
/**
 * Check if array contains an item using strict equality
 */
Object.defineProperty(Array.prototype, 'contains', {
    value: function (item) {
        return this.indexOf(item) >= 0;
    },
    enumerable: false,
    writable: true,
    configurable: true
});
// ============================================================================
// Array Manipulation
// ============================================================================
/**
 * Append items to array (mutates array)
 */
Object.defineProperty(Array.prototype, 'append', {
    value: function (item) {
        if (Array.isArray(item)) {
            this.push(...item);
        }
        else {
            this.push(item);
        }
        return this;
    },
    enumerable: false,
    writable: true,
    configurable: true
});
/**
 * Clear all elements from the array
 */
Object.defineProperty(Array.prototype, 'clear', {
    value: function () {
        this.length = 0;
    },
    enumerable: false,
    writable: true,
    configurable: true
});
/**
 * Remove all null and undefined elements
 */
Object.defineProperty(Array.prototype, 'compact', {
    value: function () {
        for (let i = this.length - 1; i >= 0; i--) {
            if (this[i] === null || this[i] === undefined) {
                this.splice(i, 1);
            }
        }
        return this;
    },
    enumerable: false,
    writable: true,
    configurable: true
});
/**
 * Remove elements from start to end (inclusive)
 * Negative indices measure from the end of the array
 */
Object.defineProperty(Array.prototype, 'remove', {
    value: function (start, end = -1) {
        if (start < 0) {
            start += this.length;
        }
        if (end < 0) {
            end += this.length;
        }
        this.splice(start, end - start + 1);
    },
    enumerable: false,
    writable: true,
    configurable: true
});
// ============================================================================
// Transformation Methods
// ============================================================================
/**
 * Transform array elements in-place
 * Modifies the original array
 */
Object.defineProperty(Array.prototype, 'transform', {
    value: function (fn) {
        for (let i = 0; i < this.length; i++) {
            this[i] = fn(this[i], i, this);
        }
        return this;
    },
    enumerable: false,
    writable: true,
    configurable: true
});
/**
 * Clone the array
 * @param deep If true, recursively clone nested arrays and objects
 */
Object.defineProperty(Array.prototype, 'clone', {
    value: function (deep = true) {
        if (!deep) {
            return [...this];
        }
        // Deep clone
        return this.map(item => {
            if (Array.isArray(item)) {
                return item.clone(true);
            }
            else if (item && typeof item === 'object' && item.constructor === Object) {
                // Clone plain objects
                const cloned = {};
                for (const key in item) {
                    if (Object.prototype.hasOwnProperty.call(item, key)) {
                        const value = item[key];
                        if (Array.isArray(value)) {
                            cloned[key] = value.clone(true);
                        }
                        else if (value && typeof value === 'object' && value.constructor === Object) {
                            cloned[key] = Object.assign({}, value);
                        }
                        else {
                            cloned[key] = value;
                        }
                    }
                }
                return cloned;
            }
            return item;
        });
    },
    enumerable: false,
    writable: true,
    configurable: true
});
/**
 * Remove duplicate elements
 */
Object.defineProperty(Array.prototype, 'unique', {
    value: function () {
        return Array.from(new Set(this));
    },
    enumerable: false,
    writable: true,
    configurable: true
});
// ============================================================================
// Filtering Methods
// ============================================================================
/**
 * Find all elements matching the predicate
 * Alias for filter()
 */
Object.defineProperty(Array.prototype, 'findAll', {
    value: function (match) {
        return this.filter(match);
    },
    enumerable: false,
    writable: true,
    configurable: true
});
/**
 * Find all elements that do NOT match the predicate
 * Opposite of filter()
 */
Object.defineProperty(Array.prototype, 'reject', {
    value: function (match) {
        const result = [];
        for (let i = 0; i < this.length; i++) {
            if (!match(this[i], i, this)) {
                result.push(this[i]);
            }
        }
        return result;
    },
    enumerable: false,
    writable: true,
    configurable: true
});
export {};
//# sourceMappingURL=ArrayExtensions.js.map