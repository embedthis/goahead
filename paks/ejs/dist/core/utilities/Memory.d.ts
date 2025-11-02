/**
 * Memory - Memory statistics and management
 *
 * Provides memory usage information
 * @spec ejs
 * @stability prototype
 */
export declare class Memory {
    /**
     * Get current resident memory usage in bytes
     */
    static get resident(): number;
    /**
     * Get heap memory used in bytes
     */
    static get heap(): number;
    /**
     * Get total heap size in bytes
     */
    static get heapTotal(): number;
    /**
     * Get external memory usage in bytes
     */
    static get external(): number;
    /**
     * Get memory statistics
     */
    static get stats(): {
        rss: number;
        heapTotal: number;
        heapUsed: number;
        external: number;
    };
    /**
     * Format bytes to human-readable string
     * @param bytes Number of bytes
     * @returns Formatted string (e.g., "1.5 MB")
     */
    static format(bytes: number): string;
}
//# sourceMappingURL=Memory.d.ts.map