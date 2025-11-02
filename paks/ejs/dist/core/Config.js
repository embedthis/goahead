/**
 * Config - Build and platform configuration
 *
 * Provides compile-time and runtime configuration information
 * @spec ejs
 * @stability evolving
 */
export class Config {
    /**
     * Product name
     */
    static Product = 'ejscript';
    /**
     * Product version
     */
    static Version = '0.1.0';
    /**
     * Operating system name
     * Values: linux, darwin, windows, freebsd, etc.
     */
    static OS = process.platform;
    /**
     * CPU architecture
     * Values: x64, arm64, ia32, etc.
     */
    static CPU = process.arch;
    /**
     * Number of CPU cores
     */
    static NumCPU = navigator.hardwareConcurrency || 1;
    /**
     * Debug build flag
     */
    static Debug = process.env.NODE_ENV === 'development';
    /**
     * Legacy compatibility mode
     */
    static Legacy = false;
    /**
     * Database support enabled
     */
    static DB = false;
    /**
     * Web framework support enabled
     */
    static WEB = false;
    /**
     * Build date
     */
    static BuildDate = new Date();
    /**
     * Get a configuration value by name
     * @param name Configuration property name
     * @returns Configuration value or undefined
     */
    static get(name) {
        return Config[name];
    }
}
//# sourceMappingURL=Config.js.map