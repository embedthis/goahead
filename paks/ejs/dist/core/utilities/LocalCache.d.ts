/**
 * LocalCache - Local in-memory cache
 *
 * Similar to Cache but for local, non-shared caching
 * @spec ejs
 * @stability evolving
 */
import { Cache, CacheOptions } from './Cache';
export declare class LocalCache extends Cache {
    /**
     * Create a local cache instance
     * @param name Cache name
     * @param options Cache options
     */
    constructor(name?: string | null, options?: CacheOptions);
}
//# sourceMappingURL=LocalCache.d.ts.map