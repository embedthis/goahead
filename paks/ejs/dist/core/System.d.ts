/**
 * System - System utilities and information
 *
 * Provides methods to interact with the operating system
 * @spec ejs
 * @stability prototype
 */
export declare class System {
    /**
     * Default buffer size
     */
    static readonly Bufsize: number;
    /**
     * Get the fully qualified system hostname
     * @returns The system hostname
     */
    static get hostname(): string;
    /**
     * Get the system IP address
     * Returns the first non-internal IPv4 address found
     * @returns The IP address or '127.0.0.1' if not found
     */
    static get ipaddr(): string;
    /**
     * Get the system temporary directory
     * @returns Path to the temporary directory
     */
    static get tmpdir(): string;
}
//# sourceMappingURL=System.d.ts.map