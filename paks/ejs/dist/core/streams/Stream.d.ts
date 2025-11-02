/**
 * Stream - Base stream interface
 *
 * Provides the foundation for all stream-based I/O
 * @spec ejs
 * @stability evolving
 */
export interface StreamOptions {
    async?: boolean;
}
/**
 * Stream direction constants
 */
export declare enum StreamDirection {
    READ = 1,
    WRITE = 2,
    BOTH = 3
}
/**
 * Base Stream interface
 * All I/O classes implement this interface
 */
export declare abstract class Stream {
    /**
     * Read direction constant
     */
    static readonly READ = StreamDirection.READ;
    /**
     * Write direction constant
     */
    static readonly WRITE = StreamDirection.WRITE;
    /**
     * Both directions constant
     */
    static readonly BOTH = StreamDirection.BOTH;
    /**
     * Is the stream in async mode
     */
    abstract get async(): boolean;
    abstract set async(enable: boolean);
    /**
     * Close the stream and free resources
     */
    abstract close(): void | Promise<void>;
    /**
     * Flush buffered data
     * @param dir Direction to flush (READ, WRITE, or BOTH)
     */
    abstract flush(dir?: number): void;
    /**
     * Read data from the stream
     * @param buffer Buffer to read into
     * @param offset Offset in buffer to start writing
     * @param count Number of bytes to read
     * @returns Number of bytes read, or null on EOF (may be async)
     */
    abstract read(buffer: Uint8Array, offset?: number, count?: number): number | null | Promise<number | null>;
    /**
     * Write data to the stream
     * @param ...args Data to write
     * @returns Number of bytes written (may be async)
     */
    abstract write(...args: any[]): number | Promise<number>;
    /**
     * Register an event observer
     * @param name Event name
     * @param observer Callback function
     */
    abstract on(name: string, observer: Function): this;
    /**
     * Remove an event observer
     * @param name Event name
     * @param observer Callback function
     */
    abstract off(name: string, observer: Function): void;
}
//# sourceMappingURL=Stream.d.ts.map