/**
 * GC - Garbage collection control
 *
 * Provides garbage collection control and statistics
 * @spec ejs
 * @stability prototype
 */
export class GC {
    /**
     * Force a garbage collection
     * Note: This requires running with --expose-gc flag
     */
    static run() {
        if (global.gc) {
            global.gc();
        }
        else if (Bun.gc) {
            Bun.gc(true);
        }
    }
    /**
     * Check if garbage collection is enabled/accessible
     */
    static get enabled() {
        return typeof global.gc === 'function' || typeof Bun.gc === 'function';
    }
    /**
     * Get GC statistics (if available)
     */
    static get stats() {
        // Bun/Node doesn't provide detailed GC stats without native modules
        return {
            enabled: GC.enabled
        };
    }
}
//# sourceMappingURL=GC.js.map