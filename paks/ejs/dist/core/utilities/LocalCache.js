/**
 * LocalCache - Local in-memory cache
 *
 * Similar to Cache but for local, non-shared caching
 * @spec ejs
 * @stability evolving
 */
import { Cache } from './Cache';
export class LocalCache extends Cache {
    /**
     * Create a local cache instance
     * @param name Cache name
     * @param options Cache options
     */
    constructor(name = null, options = {}) {
        super(name, { ...options, shared: false });
    }
}
//# sourceMappingURL=LocalCache.js.map