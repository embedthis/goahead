/**
 * Memory - Memory statistics and management
 *
 * Provides memory usage information
 * @spec ejs
 * @stability prototype
 */
export class Memory {
    /**
     * Get current resident memory usage in bytes
     */
    static get resident() {
        const usage = process.memoryUsage();
        return usage.rss;
    }
    /**
     * Get heap memory used in bytes
     */
    static get heap() {
        const usage = process.memoryUsage();
        return usage.heapUsed;
    }
    /**
     * Get total heap size in bytes
     */
    static get heapTotal() {
        const usage = process.memoryUsage();
        return usage.heapTotal;
    }
    /**
     * Get external memory usage in bytes
     */
    static get external() {
        const usage = process.memoryUsage();
        return usage.external;
    }
    /**
     * Get memory statistics
     */
    static get stats() {
        return process.memoryUsage();
    }
    /**
     * Format bytes to human-readable string
     * @param bytes Number of bytes
     * @returns Formatted string (e.g., "1.5 MB")
     */
    static format(bytes) {
        const units = ['B', 'KB', 'MB', 'GB', 'TB'];
        let value = bytes;
        let unitIndex = 0;
        while (value >= 1024 && unitIndex < units.length - 1) {
            value /= 1024;
            unitIndex++;
        }
        return `${value.toFixed(2)} ${units[unitIndex]}`;
    }
}
//# sourceMappingURL=Memory.js.map