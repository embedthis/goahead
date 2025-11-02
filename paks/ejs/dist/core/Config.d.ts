/**
 * Config - Build and platform configuration
 *
 * Provides compile-time and runtime configuration information
 * @spec ejs
 * @stability evolving
 */
export declare class Config {
    /**
     * Product name
     */
    static readonly Product: string;
    /**
     * Product version
     */
    static readonly Version: string;
    /**
     * Operating system name
     * Values: linux, darwin, windows, freebsd, etc.
     */
    static readonly OS: string;
    /**
     * CPU architecture
     * Values: x64, arm64, ia32, etc.
     */
    static readonly CPU: string;
    /**
     * Number of CPU cores
     */
    static readonly NumCPU: number;
    /**
     * Debug build flag
     */
    static readonly Debug: boolean;
    /**
     * Legacy compatibility mode
     */
    static readonly Legacy: boolean;
    /**
     * Database support enabled
     */
    static readonly DB: boolean;
    /**
     * Web framework support enabled
     */
    static readonly WEB: boolean;
    /**
     * Build date
     */
    static readonly BuildDate: Date;
    /**
     * Get a configuration value by name
     * @param name Configuration property name
     * @returns Configuration value or undefined
     */
    static get(name: string): any;
}
//# sourceMappingURL=Config.d.ts.map