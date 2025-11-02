/**
 * MprLog - MPR logging subsystem
 *
 * Provides low-level access to application log file
 * @spec ejs
 * @stability evolving
 */
export declare class MprLog {
    private _level;
    private _location;
    private _fixed;
    /**
     * Get/set log level
     */
    get level(): number;
    set level(value: number);
    /**
     * Get/set log location
     */
    get location(): string;
    set location(value: string);
    /**
     * Check if log is fixed (set via command line)
     */
    get fixed(): boolean;
    /**
     * Redirect log output
     * @param location New output location
     * @param level New log level
     */
    redirect(location: string, level: number): void;
    /**
     * Write to the log
     * @param ...args Message parts
     */
    write(...args: any[]): void;
}
//# sourceMappingURL=MprLog.d.ts.map