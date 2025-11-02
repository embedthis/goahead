/**
 * Logger - Application logging
 *
 * Provides structured logging with levels and output control.
 * Loggers can direct output to Streams and may be aggregated in a hierarchical manner.
 * @spec ejs
 * @stability evolving
 */
type AppInstance = {
    outputStream: any;
    errorStream: any;
};
/**
 * Logger objects provide a convenient and consistent method to capture and store logging information.
 * Loggers can direct output to Streams and may be aggregated in a hierarchical manner.
 * Each logger may define a filter function that returns true or false depending on whether a specific message
 * should be logged or not. A matching pattern can alternatively be used to filter messages.
 */
export declare class Logger {
    /** Logging level for no logging */
    static readonly Off: number;
    /** Logging level for most serious errors */
    static readonly Error: number;
    /** Logging level for warnings */
    static readonly Warn: number;
    /** Logging level for informational messages */
    static readonly Info: number;
    /** Logging level for configuration output */
    static readonly Config: number;
    /** Logging level to output all messages */
    static readonly All: number;
    private _name;
    private _level;
    private _location;
    private _outStream;
    private _fileOpenPromise;
    private _pendingWrites;
    private _filter?;
    private _pattern?;
    private _app;
    /**
     * Logger constructor
     * @param name Unique name of the logger
     * @param location Optional output stream or Logger to send messages to
     * @param level Optional integer verbosity level (0-9)
     * @param app Optional App instance (to avoid circular dependency)
     */
    constructor(name: string, location?: any, level?: number, app?: AppInstance | null);
    /**
     * Redirect log output
     * @param location Output stream, Logger, or location specification (file:level)
     * @param level Optional new log level
     */
    redirect(location: any, level?: number | null): void;
    /**
     * Async mode - not supported for Loggers
     */
    get async(): boolean;
    set async(_enable: boolean);
    /**
     * Close the logger
     * Returns a Promise if the underlying stream requires async close (File),
     * otherwise returns void for synchronous close
     */
    close(): void | Promise<void>;
    /**
     * Filter function for this logger
     * The filter is called with signature: filter(log, name, level, kind, msg)
     */
    get filter(): ((log: Logger, name: string, level: number, kind: string, msg: string) => boolean) | undefined;
    set filter(fn: ((log: Logger, name: string, level: number, kind: string, msg: string) => boolean) | undefined);
    /**
     * Flush output stream
     * @param dir Direction (ignored, for Stream interface compatibility)
     */
    flush(dir?: number): void;
    /**
     * The numeric verbosity setting (0-9) of this logger
     */
    get level(): number;
    set level(level: number);
    /**
     * The logging location parameter specified when constructing or redirecting the logger
     */
    get location(): any;
    /**
     * Matching expression to filter log messages by logger name
     */
    get match(): RegExp | undefined;
    set match(pattern: RegExp | undefined);
    /**
     * The name of this logger
     */
    get name(): string;
    set name(name: string);
    /**
     * The output stream used by the logger
     */
    get outStream(): any;
    set outStream(stream: any);
    /**
     * Emit a debug message
     * @param level The level of the message
     * @param msgs The messages to log
     */
    debug(level: number, ...msgs: any[]): void;
    /**
     * Emit a configuration message
     * @param msgs Data to log
     */
    config(...msgs: any[]): void;
    /**
     * Emit an error message
     * @param msgs Data to log
     */
    error(...msgs: any[]): void;
    /**
     * Emit an informational message
     * @param msgs Data to log
     */
    info(...msgs: any[]): void;
    /**
     * Emit an activity message
     * @param tag Activity tag to prefix the message
     * @param args Output string to log
     */
    activity(tag: string, ...args: any[]): void;
    /**
     * Event emitter interface - not supported
     */
    off(_name: string, _observer: Function): void;
    /**
     * Event emitter interface - not supported
     */
    on(_name: string, _observer: Function): Logger;
    /**
     * Read from logger - not supported
     */
    read(_buffer: any, _offset?: number, _count?: number): number | null;
    /**
     * Write messages to the logger stream
     * NOTE: I/O errors will not throw exceptions
     * @param data Data to write
     * @returns Number of bytes written
     */
    write(...data: any[]): number;
    /**
     * Emit a warning message
     * @param msgs The data to log
     */
    warn(...msgs: any[]): void;
    /**
     * Emit a message - internal method
     * The message level will be compared to the logger setting to determine whether it will be
     * output or not. Also, if the logger has a filter function set that may filter the message.
     * @param origin Name of the logger that originated the message
     * @param level The level of the message
     * @param kind Message kind (debug, info, warn, error, config)
     * @param msg The string message to emit
     */
    private emit;
    /**
     * Alias methods for compatibility
     */
    log(...args: any[]): void;
    trace(...args: any[]): void;
}
export {};
//# sourceMappingURL=Logger.d.ts.map