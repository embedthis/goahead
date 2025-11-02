/**
 * Cache - In-memory caching
 *
 * Provides in-memory caching with TTL support
 * @spec ejs
 * @stability evolving
 */
export interface CacheOptions {
    shared?: boolean;
    lifespan?: number;
}
export declare class Cache {
    private store;
    private options;
    /**
     * Create a cache instance
     * @param name Cache name (unused in this implementation)
     * @param options Cache options
     */
    constructor(name: string | null, options?: CacheOptions);
    /**
     * Read a value from cache
     * @param key Cache key
     * @returns Cached value or null if not found or expired
     */
    read(key: string): any;
    /**
     * Read an object from cache (deserialize)
     * @param key Cache key
     * @returns Cached object or null
     */
    readObj(key: string): any;
    /**
     * Write a value to cache
     * @param key Cache key
     * @param value Value to cache
     * @param options Write options (lifespan, etc.)
     */
    write(key: string, value: any, options?: CacheOptions): void;
    /**
     * Write an object to cache (serialize)
     * @param key Cache key
     * @param value Object to cache
     * @param options Write options
     */
    writeObj(key: string, value: any, options?: CacheOptions): void;
    /**
     * Remove a value from cache
     * @param key Cache key
     */
    remove(key: string): void;
    /**
     * Expire a cache entry at a specific time
     * @param key Cache key
     * @param when When to expire (Date) or null to expire now
     */
    expire(key: string, when: Date | null): void;
    /**
     * Clear all cache entries
     */
    clear(): void;
    /**
     * Destroy the cache (alias for clear)
     */
    destroy(): void;
    /**
     * Increment a key's value by a given amount (atomic operation)
     * @param key Cache key
     * @param amount Amount to increment (can be negative for decrement)
     * @returns New value after increment, or amount if key didn't exist
     */
    inc(key: string, amount?: number): number;
    /**
     * Get cache statistics
     */
    get stats(): {
        entries: number;
    };
    /**
     * Get resource limits
     * Note: In-memory implementation has no enforced limits
     */
    get limits(): Record<string, number>;
    /**
     * Update cache resource limits
     * @param limits Object with limit properties
     * Note: In-memory implementation doesn't enforce limits, but stores config
     */
    setLimits(limits: Record<string, number>): void;
}
//# sourceMappingURL=Cache.d.ts.map