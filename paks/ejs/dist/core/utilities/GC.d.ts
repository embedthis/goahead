/**
 * GC - Garbage collection control
 *
 * Provides garbage collection control and statistics
 * @spec ejs
 * @stability prototype
 */
export declare class GC {
    /**
     * Force a garbage collection
     * Note: This requires running with --expose-gc flag
     */
    static run(): void;
    /**
     * Check if garbage collection is enabled/accessible
     */
    static get enabled(): boolean;
    /**
     * Get GC statistics (if available)
     */
    static get stats(): any;
}
//# sourceMappingURL=GC.d.ts.map