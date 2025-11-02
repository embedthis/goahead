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
import { Config } from './Config';
/**
 * Standard I/O stream wrapper for process streams
 */
class ProcessStream extends Stream {
    writable;
    _readable;
    _async = false;
    constructor(stream) {
        super();
        if ('write' in stream) {
            this.writable = stream;
        }
        if ('read' in stream) {
            this._readable = stream;
        }
    }
    get async() {
        return this._async;
    }
    set async(enable) {
        this._async = enable;
    }
    close() {
        // Don't close standard streams
    }
    flush(_dir) {
        // Standard streams auto-flush
    }
    read(_buffer, _offset = 0, _count = -1) {
        // Simplified synchronous read - would need proper implementation
        return null;
    }
    write(...args) {
        if (!this.writable) {
            throw new Error('Stream is not writable');
        }
        let totalWritten = 0;
        for (const arg of args) {
            const str = String(arg);
            this.writable.write(str);
            totalWritten += str.length;
        }
        return totalWritten;
    }
    on(_name, _observer) {
        return this;
    }
    off(_name, _observer) {
    }
}
export class App {
    /**
     * Application configuration loaded from ejsrc
     */
    static config = {};
    /**
     * Search path separator for this platform
     */
    static SearchSeparator = process.platform === 'win32' ? ';' : ':';
    /**
     * Default ejsrc configuration
     */
    static defaultConfig = {
        log: {
            enable: true,
            location: 'stderr',
            level: 0
        },
        app: {
            reload: false
        },
        cache: {
            app: { enable: false }
        },
        dirs: {
            cache: new Path('cache')
        },
        files: {
            ejsrc: new Path('ejsrc')
        },
        init: {},
        uris: {},
        test: {}
    };
    /**
     * Default event emitter for the application
     */
    static emitter = new Emitter();
    /**
     * Application logger
     */
    static log;
    /**
     * MPR log object
     */
    static mprLog;
    /**
     * Application name
     */
    static name;
    /**
     * Application title
     */
    static title;
    /**
     * Application version
     */
    static version;
    /**
     * Application in-memory cache
     */
    static cache;
    /**
     * Application start time
     */
    static started = new Date();
    /**
     * Test object for unit tests
     */
    static test;
    // Standard I/O streams
    static _errorStream;
    static _inputStream;
    static _outputStream;
    /**
     * Standard output text stream
     */
    static stdout;
    /**
     * Standard input text stream
     */
    static stdin;
    /**
     * Standard error text stream
     */
    static stderr;
    /**
     * Get command-line arguments
     */
    static get args() {
        return process.argv.slice(1); // Skip node/bun executable
    }
    /**
     * Change working directory
     * @param value New working directory path
     */
    static chdir(value) {
        const path = value instanceof Path ? value.name : value;
        process.chdir(path);
    }
    /**
     * Create a search path array
     * @param searchPath Colon/semicolon separated search path
     * @returns Array of Path objects
     */
    static createSearch(searchPath) {
        if (!searchPath) {
            // Return default search paths
            return [
                new Path('.'),
                new Path(process.cwd())
            ];
        }
        const sep = this.SearchSeparator;
        return searchPath.split(sep).map(p => new Path(p));
    }
    /**
     * Get current working directory
     */
    static get dir() {
        return new Path(process.cwd());
    }
    /**
     * Get environment variables (returns a copy for read-only access)
     */
    static get env() {
        return { ...process.env };
    }
    /**
     * Get executable directory
     */
    static get exeDir() {
        return new Path(process.execPath).dirname;
    }
    /**
     * Get executable path
     */
    static get exePath() {
        return new Path(process.execPath);
    }
    /**
     * Get/set standard error stream
     */
    static get errorStream() {
        return this._errorStream;
    }
    static set errorStream(stream) {
        this._errorStream = stream;
        if (this.stderr) {
            this.stderr.close();
        }
        this.stderr = new TextStream(stream);
    }
    /**
     * Exit the application
     * @param status Exit status code
     * @param how How to exit (abort, normal, safe)
     * @param timeout Timeout in milliseconds
     * @returns True if exited, false if cancelled
     */
    static exit(status = 0, how = 'normal', timeout = 0) {
        if (how === 'abort') {
            process.exit(status);
        }
        else if (how === 'normal') {
            // Allow cleanup
            setTimeout(() => process.exit(status), 0);
        }
        else if (how === 'safe') {
            // Try to exit gracefully
            setTimeout(() => {
                // Could return false if still busy
                process.exit(status);
            }, timeout);
        }
        return true;
    }
    /**
     * Get an environment variable
     * @param name Variable name
     * @returns Variable value or null
     */
    static getenv(name) {
        return process.env[name] || null;
    }
    /**
     * Get group ID (Unix only)
     */
    static get gid() {
        return process.getgid ? process.getgid() : -1;
    }
    /**
     * Get user's home directory
     */
    static get home() {
        const { homedir } = require('os');
        return new Path(homedir());
    }
    /**
     * Get/set standard input stream
     */
    static get inputStream() {
        return this._inputStream;
    }
    static set inputStream(stream) {
        this._inputStream = stream;
        if (this.stdin) {
            this.stdin.close();
        }
        this.stdin = new TextStream(stream);
    }
    /**
     * Load an ejsrc configuration file
     * @param path Path to config file (optional, defaults to 'ejsrc')
     * @param overwrite If true, overwrite existing config values
     * @returns Configuration object
     */
    static async loadrc(path, overwrite = true) {
        if (!path) {
            return this.config;
        }
        const configPath = path instanceof Path ? path : new Path(path);
        if (configPath.exists) {
            try {
                const content = await configPath.readString();
                if (content) {
                    const config = JSON.parse(content);
                    this.blendConfig(this.config, config, overwrite);
                }
            }
            catch (e) {
                console.error(`${this.exePath.basename}: Cannot parse ${configPath}: ${e}`);
            }
        }
        return this.config;
    }
    /**
     * Get/set standard output stream
     */
    static get outputStream() {
        return this._outputStream;
    }
    static set outputStream(stream) {
        this._outputStream = stream;
        if (this.stdout) {
            this.stdout.close();
        }
        this.stdout = new TextStream(stream);
    }
    /**
     * Get process ID
     */
    static get pid() {
        return process.pid;
    }
    /**
     * Set an environment variable
     * @param name Variable name
     * @param value Variable value
     */
    static putenv(name, value) {
        process.env[name] = value;
    }
    /**
     * Execute a shell command and return its output
     * @param command Shell command to execute
     * @returns Command output as string
     */
    static run(command) {
        const { execSync } = require('child_process');
        try {
            const output = execSync(command, { encoding: 'utf-8' });
            return output;
        }
        catch (e) {
            return e.stdout || '';
        }
    }
    /**
     * Execute a system command and return exit code
     * @param command Shell command to execute
     * @returns Exit code (0 for success)
     */
    static system(command) {
        const { spawnSync } = require('child_process');
        try {
            const result = spawnSync(command, { shell: true, stdio: 'inherit' });
            return result.status || 0;
        }
        catch (e) {
            return 1;
        }
    }
    /**
     * Sleep for the given milliseconds
     * @param delay Time in milliseconds
     */
    static sleep(delay = -1) {
        if (delay < 0) {
            // Sleep forever - just block
            while (true) {
                Bun.sleepSync(1000);
            }
        }
        else {
            Bun.sleepSync(delay);
        }
    }
    /**
     * Get/set module search path
     */
    static get search() {
        const modulePathsEnv = this.getenv('NODE_PATH') || '';
        if (!modulePathsEnv) {
            return [];
        }
        return this.createSearch(modulePathsEnv);
    }
    static set search(paths) {
        if (paths.length === 0) {
            this.putenv('NODE_PATH', '');
        }
        else {
            const pathString = paths.map(p => p.name).join(this.SearchSeparator);
            this.putenv('NODE_PATH', pathString);
        }
    }
    /**
     * Get user ID (Unix only)
     */
    static get uid() {
        return process.getuid ? process.getuid() : -1;
    }
    /**
     * Get password from user (Unix tty)
     * @param prompt Prompt string
     * @returns Password string
     */
    static getpass(prompt) {
        // Simplified - would need tty manipulation
        process.stdout.write(prompt);
        return '';
    }
    /**
     * Update log based on configuration
     */
    static updateLog() {
        const log = this.config.log;
        if (log && log.enable && !this.mprLog.fixed) {
            this.log.redirect(log.location, log.level);
            this.mprLog.redirect(log.location, log.level);
        }
    }
    /**
     * Wait for an event
     * @param obj Observable object
     * @param events Events to wait for
     * @param timeout Timeout in milliseconds
     * @returns True if event occurred
     */
    static waitForEvent(obj, events, timeout = Number.MAX_SAFE_INTEGER) {
        let done = false;
        const callback = (_event) => { done = true; };
        obj.on(events, callback);
        const start = Date.now();
        while (!done && (Date.now() - start) < timeout) {
            // Simple delay to allow event processing
            const delay = Math.min(10, timeout - (Date.now() - start));
            if (delay > 0) {
                Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, delay);
            }
        }
        obj.off(events, callback);
        return done;
    }
    /**
     * Blend configuration objects
     */
    static blendConfig(dest, src, overwrite) {
        for (const key in src) {
            if (overwrite || !(key in dest)) {
                if (typeof src[key] === 'object' && !Array.isArray(src[key]) && src[key] !== null) {
                    if (typeof dest[key] !== 'object') {
                        dest[key] = {};
                    }
                    this.blendConfig(dest[key], src[key], overwrite);
                }
                else {
                    dest[key] = src[key];
                }
            }
        }
        return dest;
    }
    /**
     * Initialize the App singleton
     * Called automatically when module loads
     * @param initialConfig Optional initial configuration to merge
     */
    static init(initialConfig) {
        // Set application name and version
        this.name = new Path(this.args[0] || Config.Product).basename.name;
        this.title = this.name;
        this.version = Config.Version;
        this.mprLog = new MprLog();
        // Load configuration
        this.config = {};
        // Merge initial config if provided
        if (initialConfig) {
            this.blendConfig(this.config, initialConfig, true);
        }
        // Only load local ejsrc, not from home directory
        // const homeDir = this.getenv('HOME')
        // if (homeDir) {
        //     this.loadrc(new Path(homeDir).join('.ejsrc'))
        // }
        this.loadrc('ejsrc');
        this.blendConfig(this.config, this.defaultConfig, false);
        // Setup standard I/O streams
        this._outputStream = new ProcessStream(process.stdout);
        this._errorStream = new ProcessStream(process.stderr);
        this._inputStream = new ProcessStream(process.stdin);
        this.stdout = new TextStream(this._outputStream);
        this.stderr = new TextStream(this._errorStream);
        this.stdin = new TextStream(this._inputStream);
        // Setup logging
        const log = this.config.log;
        if (log && log.enable) {
            const level = this.mprLog.fixed ? this.mprLog.level : log.level;
            const location = this.mprLog.fixed ? this.mprLog : log.location;
            if (!this.mprLog.fixed) {
                this.mprLog.redirect(log.location, level);
            }
            this.log = new Logger(this.name, location, level, this);
        }
        // Setup cache
        if (this.config.cache) {
            this.cache = new Cache(null, { shared: true, ...this.config.cache });
        }
        // Make directories available
        this.config.directories = this.config.dirs;
    }
}
const isTestContext = typeof describe !== 'undefined' && typeof it !== 'undefined';
if (!isTestContext) {
    App.init();
}
//# sourceMappingURL=App.js.map