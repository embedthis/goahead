/**
 * Cache - In-memory caching
 *
 * Provides in-memory caching with TTL support
 * @spec ejs
 * @stability evolving
 */
export class Cache {
    store;
    options;
    /**
     * Create a cache instance
     * @param name Cache name (unused in this implementation)
     * @param options Cache options
     */
    constructor(name, options = {}) {
        this.store = new Map();
        this.options = options;
    }
    /**
     * Read a value from cache
     * @param key Cache key
     * @returns Cached value or null if not found or expired
     */
    read(key) {
        const entry = this.store.get(key);
        if (!entry) {
            return null;
        }
        // Check expiration
        if (entry.expires !== null && Date.now() > entry.expires) {
            this.store.delete(key);
            return null;
        }
        return entry.value;
    }
    /**
     * Read an object from cache (deserialize)
     * @param key Cache key
     * @returns Cached object or null
     */
    readObj(key) {
        const value = this.read(key);
        if (value === null) {
            return null;
        }
        // If it's a string, try to parse as JSON
        if (typeof value === 'string') {
            try {
                return JSON.parse(value);
            }
            catch {
                return value;
            }
        }
        return value;
    }
    /**
     * Write a value to cache
     * @param key Cache key
     * @param value Value to cache
     * @param options Write options (lifespan, etc.)
     */
    write(key, value, options) {
        const lifespan = options?.lifespan ?? this.options.lifespan;
        const expires = lifespan ? Date.now() + (lifespan * 1000) : null;
        this.store.set(key, {
            value,
            expires
        });
    }
    /**
     * Write an object to cache (serialize)
     * @param key Cache key
     * @param value Object to cache
     * @param options Write options
     */
    writeObj(key, value, options) {
        this.write(key, value, options);
    }
    /**
     * Remove a value from cache
     * @param key Cache key
     */
    remove(key) {
        this.store.delete(key);
    }
    /**
     * Expire a cache entry at a specific time
     * @param key Cache key
     * @param when When to expire (Date) or null to expire now
     */
    expire(key, when) {
        if (when === null) {
            this.store.delete(key);
        }
        else {
            const entry = this.store.get(key);
            if (entry) {
                entry.expires = when.getTime();
                this.store.set(key, entry);
            }
        }
    }
    /**
     * Clear all cache entries
     */
    clear() {
        this.store.clear();
    }
    /**
     * Destroy the cache (alias for clear)
     */
    destroy() {
        this.clear();
    }
    /**
     * Increment a key's value by a given amount (atomic operation)
     * @param key Cache key
     * @param amount Amount to increment (can be negative for decrement)
     * @returns New value after increment, or amount if key didn't exist
     */
    inc(key, amount = 1) {
        const entry = this.store.get(key);
        let currentValue = 0;
        if (entry) {
            // Check if expired
            if (entry.expires !== null && Date.now() > entry.expires) {
                this.store.delete(key);
            }
            else {
                // Parse current value as number
                currentValue = typeof entry.value === 'number' ? entry.value : parseFloat(entry.value) || 0;
            }
        }
        const newValue = currentValue + amount;
        // Store with same expiry as original, or use default lifespan
        const expires = entry?.expires || (this.options.lifespan ? Date.now() + (this.options.lifespan * 1000) : null);
        this.store.set(key, {
            value: newValue,
            expires
        });
        return newValue;
    }
    /**
     * Get cache statistics
     */
    get stats() {
        return {
            entries: this.store.size
        };
    }
    /**
     * Get resource limits
     * Note: In-memory implementation has no enforced limits
     */
    get limits() {
        return {
            keys: 0, // No limit
            memory: 0, // No limit
            lifespan: this.options.lifespan || 0
        };
    }
    /**
     * Update cache resource limits
     * @param limits Object with limit properties
     * Note: In-memory implementation doesn't enforce limits, but stores config
     */
    setLimits(limits) {
        if (limits.lifespan !== undefined) {
            this.options.lifespan = limits.lifespan;
        }
        // Other limits (keys, memory) would be enforced in a full implementation
    }
}
//# sourceMappingURL=Cache.js.map