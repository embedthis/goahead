/**
 * App - Application singleton
 *
 * Provides application configuration state and controls the execution environment
 * @spec ejs
 * @stability evolving
 */
import { Path } from './Path';
import { TextStream } from './streams/TextStream';
import { Stream } from './streams/Stream';
import { Logger } from './utilities/Logger';
import { MprLog } from './utilities/MprLog';
import { Emitter } from './async/Emitter';
import { Cache } from './utilities/Cache';
export declare class App {
    /**
     * Application configuration loaded from ejsrc
     */
    static config: any;
    /**
     * Search path separator for this platform
     */
    static readonly SearchSeparator: string;
    /**
     * Default ejsrc configuration
     */
    private static defaultConfig;
    /**
     * Default event emitter for the application
     */
    static emitter: Emitter;
    /**
     * Application logger
     */
    static log: Logger;
    /**
     * MPR log object
     */
    static mprLog: MprLog;
    /**
     * Application name
     */
    static name: string;
    /**
     * Application title
     */
    static title: string;
    /**
     * Application version
     */
    static version: string;
    /**
     * Application in-memory cache
     */
    static cache: Cache;
    /**
     * Application start time
     */
    static readonly started: Date;
    /**
     * Test object for unit tests
     */
    static test: any;
    private static _errorStream;
    private static _inputStream;
    private static _outputStream;
    /**
     * Standard output text stream
     */
    static stdout: TextStream;
    /**
     * Standard input text stream
     */
    static stdin: TextStream;
    /**
     * Standard error text stream
     */
    static stderr: TextStream;
    /**
     * Get command-line arguments
     */
    static get args(): string[];
    /**
     * Change working directory
     * @param value New working directory path
     */
    static chdir(value: string | Path): void;
    /**
     * Create a search path array
     * @param searchPath Colon/semicolon separated search path
     * @returns Array of Path objects
     */
    static createSearch(searchPath?: string | null): Path[];
    /**
     * Get current working directory
     */
    static get dir(): Path;
    /**
     * Get environment variables (returns a copy for read-only access)
     */
    static get env(): Record<string, string>;
    /**
     * Get executable directory
     */
    static get exeDir(): Path;
    /**
     * Get executable path
     */
    static get exePath(): Path;
    /**
     * Get/set standard error stream
     */
    static get errorStream(): Stream;
    static set errorStream(stream: Stream);
    /**
     * Exit the application
     * @param status Exit status code
     * @param how How to exit (abort, normal, safe)
     * @param timeout Timeout in milliseconds
     * @returns True if exited, false if cancelled
     */
    static exit(status?: number, how?: string, timeout?: number): boolean;
    /**
     * Get an environment variable
     * @param name Variable name
     * @returns Variable value or null
     */
    static getenv(name: string): string | null;
    /**
     * Get group ID (Unix only)
     */
    static get gid(): number;
    /**
     * Get user's home directory
     */
    static get home(): Path;
    /**
     * Get/set standard input stream
     */
    static get inputStream(): Stream;
    static set inputStream(stream: Stream);
    /**
     * Load an ejsrc configuration file
     * @param path Path to config file (optional, defaults to 'ejsrc')
     * @param overwrite If true, overwrite existing config values
     * @returns Configuration object
     */
    static loadrc(path?: Path | string, overwrite?: boolean): Promise<any>;
    /**
     * Get/set standard output stream
     */
    static get outputStream(): Stream;
    static set outputStream(stream: Stream);
    /**
     * Get process ID
     */
    static get pid(): number;
    /**
     * Set an environment variable
     * @param name Variable name
     * @param value Variable value
     */
    static putenv(name: string, value: string): void;
    /**
     * Execute a shell command and return its output
     * @param command Shell command to execute
     * @returns Command output as string
     */
    static run(command: string): string;
    /**
     * Execute a system command and return exit code
     * @param command Shell command to execute
     * @returns Exit code (0 for success)
     */
    static system(command: string): number;
    /**
     * Sleep for the given milliseconds
     * @param delay Time in milliseconds
     */
    static sleep(delay?: number): void;
    /**
     * Get/set module search path
     */
    static get search(): Path[];
    static set search(paths: Path[]);
    /**
     * Get user ID (Unix only)
     */
    static get uid(): number;
    /**
     * Get password from user (Unix tty)
     * @param prompt Prompt string
     * @returns Password string
     */
    static getpass(prompt: string): string;
    /**
     * Update log based on configuration
     */
    static updateLog(): void;
    /**
     * Wait for an event
     * @param obj Observable object
     * @param events Events to wait for
     * @param timeout Timeout in milliseconds
     * @returns True if event occurred
     */
    static waitForEvent(obj: any, events: any, timeout?: number): boolean;
    /**
     * Blend configuration objects
     */
    private static blendConfig;
    /**
     * Initialize the App singleton
     * Called automatically when module loads
     * @param initialConfig Optional initial configuration to merge
     */
    static init(initialConfig?: any): void;
}
//# sourceMappingURL=App.d.ts.map