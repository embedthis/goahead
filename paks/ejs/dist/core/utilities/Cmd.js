/**
 * Cmd - Command execution
 *
 * The Cmd class supports invoking other programs as separate processes on the same system.
 * Implements Stream interface for reading/writing to command stdin/stdout.
 * @spec ejs
 * @stability evolving
 */
import { App } from '../App';
import { Path } from '../Path';
import { Config } from '../Config';
import { ByteArray } from '../streams/ByteArray';
import { TextStream } from '../streams/TextStream';
import { Emitter } from '../async/Emitter';
/**
 * Cmd class for executing external commands with stream interface
 */
export class Cmd extends Emitter {
    _response = null;
    _errorResponse = null;
    _env;
    _timeout = 30000;
    _process = null;
    _pid = null;
    _status = null;
    _stdoutData = new ByteArray(1024 * 1024); // 1MB initial size for command output
    _stderrData = new ByteArray(64 * 1024); // 64KB for stderr
    _stdoutStream = null;
    _stderrStream = null;
    _finalized = false;
    _stdoutReader = null;
    _stderrReader = null;
    _stdoutPromise = null;
    _stderrPromise = null;
    _readingStreams = false;
    /**
     * Create a Cmd object
     * @param command Optional command line to initialize with
     * @param options Command options
     *
     * Execution mode:
     * - detach=true: Async execution (use start())
     * - exceptions=false: Async execution (use start() + wait())
     * - No options or other options: Sync execution for backward compatibility (use spawnSync)
     */
    constructor(command = null, options = {}) {
        super();
        if (options.timeout) {
            this._timeout = options.timeout;
        }
        if (command) {
            // Use async mode if explicitly requested via detach or exceptions=false
            // Otherwise use sync mode for backward compatibility
            if (options.detach || options.exceptions === false) {
                this.start(command, options);
            }
            else {
                this._startSync(command, options);
            }
        }
    }
    /**
     * Start command synchronously using Bun.spawnSync
     * This is used when the constructor is called with a command (non-detached mode)
     */
    _startSync(cmdline, options) {
        const cwd = options.dir ? (options.dir instanceof Path ? options.dir.name : options.dir) : undefined;
        // Parse command line
        let cmd;
        let args = [];
        if (typeof cmdline === 'string') {
            // String commands are wrapped in a shell to support:
            // - Shell built-ins (echo, cd, set, etc.)
            // - Shell operators (|, >, <, &&, ||, &)
            // - Wildcards/globbing (*.txt)
            // - Variable expansion ($VAR, %VAR%)
            // On Windows, use bash from Git for Windows if available, otherwise cmd.exe
            // On Unix-like systems, use /bin/sh
            if (Config.OS === 'win32' || Config.OS === 'windows' || Config.OS === 'cygwin') {
                // Try bash first (from Git for Windows), fall back to cmd.exe
                const bashPath = Cmd.locate('bash');
                if (bashPath) {
                    cmd = bashPath.toString();
                    args = ['-c', cmdline];
                }
                else {
                    cmd = 'cmd.exe';
                    args = ['/c', cmdline];
                }
            }
            else {
                cmd = '/bin/sh';
                args = ['-c', cmdline];
            }
        }
        else {
            // Array commands execute directly without shell wrapper (Unix philosophy)
            // This provides better security (no shell injection) and performance
            // Note: Arrays must use real executables, not shell built-ins
            // (e.g., use ['bash', '--version'] not ['echo', 'test'] on Windows)
            cmd = cmdline[0];
            args = cmdline.slice(1);
        }
        // Use Bun.spawnSync for synchronous execution
        const result = Bun.spawnSync([cmd, ...args], {
            cwd,
            env: this._env ? { ...process.env, ...this._env } : process.env,
            stdin: 'inherit',
            stdout: 'pipe',
            stderr: 'pipe',
        });
        this._status = result.exitCode;
        this._stdoutData.write(result.stdout);
        this._stderrData.write(result.stderr);
        this._pid = -1; // spawnSync doesn't provide PID after completion
    }
    /**
     * Close the command and free resources
     */
    close() {
        // Cancel stream readers first to stop async reading
        if (this._stdoutReader) {
            try {
                this._stdoutReader.cancel();
            }
            catch { }
            this._stdoutReader = null;
        }
        if (this._stderrReader) {
            try {
                this._stderrReader.cancel();
            }
            catch { }
            this._stderrReader = null;
        }
        this._readingStreams = false;
        if (this._process) {
            try {
                this._process.kill();
                // Unref the process so it doesn't keep event loop alive
                this._process.unref();
            }
            catch { }
            this._process = null;
        }
        this._pid = null;
        // Remove all event listeners to prevent memory leaks
        this.removeAllListeners();
    }
    /**
     * Hash of environment strings to pass to the command
     */
    get env() {
        return this._env || {};
    }
    set env(values) {
        this._env = values;
    }
    /**
     * Command error output data as a string
     */
    get error() {
        if (!this._errorResponse) {
            this._errorResponse = this._stderrData.toString();
        }
        return this._errorResponse;
    }
    /**
     * The error stream object for the command's standard error output
     */
    get errorStream() {
        if (!this._stderrStream) {
            this._stderrStream = {
                toString: () => this._stderrData.toString(),
                read: (buffer, offset = 0, count = -1) => {
                    const data = this._stderrData;
                    if (count === -1)
                        count = data.length;
                    for (let i = 0; i < count && i < data.length; i++) {
                        buffer[offset + i] = data[i];
                    }
                    return Math.min(count, data.length);
                }
            };
        }
        return this._stderrStream;
    }
    /**
     * Signal the end of writing data to the command
     */
    finalize() {
        this._finalized = true;
        if (this._process && this._process.stdin) {
            try {
                if (typeof this._process.stdin === 'object' && 'end' in this._process.stdin) {
                    this._process.stdin.end();
                }
            }
            catch { }
        }
    }
    /**
     * Flush stream (no-op for commands)
     */
    flush(_dir = 0) {
        // No-op for commands
    }
    /**
     * Process ID of the command
     */
    get pid() {
        if (this._pid === null) {
            throw new Error('Command not started or already closed');
        }
        return this._pid;
    }
    /**
     * Read data from command output
     * @param buffer ByteArray to read into
     * @param offset Offset in buffer
     * @param count Number of bytes to read
     * @returns Number of bytes read or null if no data
     */
    async read(buffer, offset = 0, count = -1) {
        // If we're still reading streams, wait for them to complete
        if (this._readingStreams && this._stdoutPromise) {
            await this._stdoutPromise;
        }
        const data = this._stdoutData;
        if (data.length === 0)
            return null;
        if (count === -1)
            count = data.length;
        let bytesRead = 0;
        for (let i = 0; i < count && i < data.length; i++) {
            buffer[offset + i] = data[i];
            bytesRead++;
        }
        // Remove read data from buffer
        if (bytesRead < data.length) {
            const remaining = data.slice(bytesRead);
            const newBuffer = new ByteArray(remaining.length);
            for (let i = 0; i < remaining.length; i++) {
                newBuffer[i] = remaining[i];
            }
            this._stdoutData = newBuffer;
        }
        else {
            // All data was read, clear the buffer
            this._stdoutData = new ByteArray();
        }
        return bytesRead;
    }
    /**
     * Read the data from the command output as a string
     * @param count Number of bytes to read (-1 for all)
     * @returns String data
     */
    async readString(count = -1) {
        // If we're still reading streams, wait for them to complete
        if (this._readingStreams && this._stdoutPromise) {
            await this._stdoutPromise;
        }
        const data = this._stdoutData.toString();
        if (count === -1) {
            this._stdoutData = new ByteArray();
            return data;
        }
        const result = data.substring(0, count);
        const remaining = Buffer.from(data.substring(count));
        const newBuffer = new ByteArray(remaining.length);
        newBuffer.write(remaining);
        this._stdoutData = newBuffer;
        return result;
    }
    /**
     * Read the data from the command as an array of lines
     * @param count Number of lines to read (-1 for all)
     * @returns Array of lines
     */
    async readLines(count = -1) {
        // If we're still reading streams, wait for them to complete
        if (this._readingStreams && this._stdoutPromise) {
            await this._stdoutPromise;
        }
        const stream = new TextStream(this);
        return stream.readLines();
    }
    /**
     * Command output data as a string (cached)
     * Returns string if data is available synchronously (from constructor execution)
     * Returns Promise if command was started with start() method
     */
    get response() {
        // If already cached, return it
        if (this._response) {
            return this._response;
        }
        // If process is not running and data is available, return synchronously
        // This handles the case where constructor was called with a command
        if (!this._readingStreams && this._stdoutData.length > 0) {
            this._response = this._stdoutData.toString();
            return this._response;
        }
        // Otherwise, async read (for commands started with start())
        return this.readString().then(result => {
            this._response = result;
            return result;
        });
    }
    /**
     * Start the command
     * @param cmdline Command line (string or array)
     * @param options Command options
     */
    start(cmdline, options = {}) {
        const detach = options.detach || false;
        const cwd = options.dir ? (options.dir instanceof Path ? options.dir.name : options.dir) : undefined;
        const timeout = options.timeout || this._timeout;
        // Parse command line
        let cmd;
        let args = [];
        if (typeof cmdline === 'string') {
            // String command - will be wrapped in shell
            cmd = cmdline;
        }
        else {
            // Array of args - direct execution without shell
            cmd = cmdline[0];
            args = cmdline.slice(1);
        }
        // String commands are wrapped in a shell to support shell features
        // Array commands execute directly for security and performance
        // On Windows, use bash from Git for Windows if available, otherwise cmd.exe
        // On Unix-like systems, use /bin/sh
        let shell;
        if (Config.OS === 'win32' || Config.OS === 'windows' || Config.OS === 'cygwin') {
            // Try bash first (from Git for Windows), fall back to cmd.exe
            const bashPath = Cmd.locate('bash');
            if (bashPath) {
                shell = [bashPath.toString(), '-c'];
            }
            else {
                shell = ['cmd.exe', '/c'];
            }
        }
        else {
            shell = ['/bin/sh', '-c'];
        }
        // Spawn process
        this._process = Bun.spawn(typeof cmdline === 'string' ? [...shell, cmd] : [cmd, ...args], {
            cwd,
            env: this._env ? { ...process.env, ...this._env } : process.env,
            stdin: detach ? 'pipe' : 'inherit',
            stdout: 'pipe',
            stderr: 'pipe',
        });
        this._pid = this._process.pid;
        // Set up stdout reading - STORE the promise so we can wait for it
        if (this._process.stdout && typeof this._process.stdout === 'object' && 'getReader' in this._process.stdout) {
            this._stdoutReader = this._process.stdout.getReader();
            this._readingStreams = true;
            this._stdoutPromise = this._readStream(this._stdoutReader, this._stdoutData, 'readable');
        }
        // Set up stderr reading - STORE the promise so we can wait for it
        if (this._process.stderr && typeof this._process.stderr === 'object' && 'getReader' in this._process.stderr) {
            this._stderrReader = this._process.stderr.getReader();
            this._readingStreams = true;
            this._stderrPromise = this._readStream(this._stderrReader, this._stderrData, 'error');
        }
        // Handle completion
        this._process.exited.then((exitCode) => {
            this._status = exitCode;
            this.emit('complete', this);
            if (options.exceptions !== false && exitCode !== 0) {
                throw new Error(`Command failed with status ${exitCode}\n${this.error}`);
            }
        }).catch(() => {
            // Silently ignore errors during cleanup (e.g., killed processes)
        });
    }
    /**
     * Read from a stream into a ByteArray
     */
    async _readStream(reader, buffer, event) {
        try {
            while (true) {
                const { done, value } = await reader.read();
                if (done)
                    break;
                // Append to buffer
                for (const byte of value) {
                    buffer.writeByte(byte);
                }
                // Emit event
                this.emit(event, this);
            }
        }
        catch (err) {
            // Silently ignore cancellation and other stream errors
            // This is expected when close() cancels the reader
        }
    }
    /**
     * Get the command exit status
     * @returns Exit status code
     *
     * Note: If the command was started in the constructor (non-detached mode),
     * the status will already be available. For detached commands, use await cmd.wait() first.
     */
    get status() {
        return this._status || 0;
    }
    /**
     * Stop the current command
     * @param signal Signal to send (default SIGINT = 2)
     * @returns True if successfully stopped
     */
    stop(signal = 2) {
        if (!this._process)
            return false;
        // Cancel stream readers to stop async operations
        if (this._stdoutReader) {
            try {
                this._stdoutReader.cancel();
            }
            catch { }
        }
        if (this._stderrReader) {
            try {
                this._stderrReader.cancel();
            }
            catch { }
        }
        try {
            this._process.kill(signal);
            return true;
        }
        catch {
            return false;
        }
    }
    /**
     * Default command timeout
     */
    get timeout() {
        return this._timeout;
    }
    set timeout(msec) {
        this._timeout = msec;
    }
    /**
     * Wait for command to complete
     * @param timeout Time in milliseconds to wait
     * @returns True if command completed
     */
    async wait(timeout = -1) {
        if (!this._process)
            return false;
        const timeoutMs = timeout === -1 ? this._timeout : timeout;
        let timeoutId = null;
        try {
            const result = await Promise.race([
                this._process.exited,
                new Promise((_, reject) => {
                    timeoutId = setTimeout(() => reject(new Error('Timeout')), timeoutMs);
                })
            ]);
            return true;
        }
        catch {
            return false;
        }
        finally {
            // CRITICAL: Clear the timeout to prevent keeping event loop alive
            if (timeoutId !== null) {
                clearTimeout(timeoutId);
            }
        }
    }
    /**
     * Write data to command stdin
     * @param data Data to write
     * @returns Number of bytes written
     */
    write(...data) {
        if (!this._process || !this._process.stdin) {
            throw new Error('Command not started in detached mode');
        }
        const str = data.map(d => String(d)).join('');
        if (typeof this._process.stdin === 'object' && 'getWriter' in this._process.stdin) {
            const writer = this._process.stdin.getWriter();
            const encoded = new TextEncoder().encode(str);
            writer.write(encoded);
            writer.releaseLock();
            return encoded.length;
        }
        return 0;
    }
    /* Static Helper Methods */
    /**
     * Locate a program using the PATH
     * @param program Program to find
     * @param search Optional additional search paths
     * @returns Located path or null
     */
    static locate(program, search = []) {
        const searchPaths = [...search, ...(App.getenv('PATH') || '').split(Config.OS === 'win32' || Config.OS === 'windows' ? ';' : ':')];
        for (const dir of searchPaths) {
            const path = new Path(dir).join(program instanceof Path ? program.name : program);
            if (path.exists && !path.isDir) {
                return path;
            }
        }
        // Windows extensions
        if (Config.OS === 'win32' || Config.OS === 'windows' || Config.OS === 'cygwin') {
            const prog = program instanceof Path ? program : new Path(program);
            if (!prog.extension) {
                for (const ext of ['exe', 'bat', 'cmd']) {
                    const result = Cmd.locate(prog.joinExt(ext), search);
                    if (result)
                        return result;
                }
            }
        }
        return null;
    }
    /**
     * Start a command as a daemon
     * @param cmdline Command line
     * @param options Command options
     * @returns Process ID
     */
    static daemon(cmdline, options = {}) {
        const cmd = new Cmd();
        cmd.start(cmdline, { ...options, detach: true });
        cmd.finalize();
        return cmd.pid;
    }
    /**
     * Execute a new program replacing current process
     * @param cmdline Command line
     * @param options Command options
     */
    static async exec(cmdline = null, options = {}) {
        if (!cmdline) {
            throw new Error('Command line required');
        }
        // Note: This cannot truly replace the process in Bun like it can in C
        // We'll simulate by running and exiting
        const result = await Cmd.run(cmdline, options);
        console.log(result);
        process.exit(0);
    }
    /**
     * Kill a process by PID
     * @param pid Process ID
     * @param signal Signal number (default SIGINT = 2)
     * @returns True if successful
     */
    static kill(pid, signal = 2) {
        try {
            process.kill(pid, signal);
            return true;
        }
        catch {
            return false;
        }
    }
    /**
     * Execute a command and return output
     * @param command Command to execute
     * @param options Command options
     * @param data Optional data to write to stdin
     * @returns Command output
     */
    static async run(command, options = {}, data = null) {
        const cmd = new Cmd();
        cmd.start(command, { ...options, detach: true, exceptions: false });
        if (options.detach) {
            return null;
        }
        if (data) {
            cmd.write(data);
        }
        cmd.finalize();
        // Wait for command to complete
        await cmd.wait();
        // Read all output
        const result = await cmd.readString();
        if (options.stream && result) {
            process.stdout.write(result);
        }
        return result;
    }
    /**
     * Run a command using the system shell
     * @param command Command to execute
     * @param options Command options
     * @param data Optional data to write to stdin
     * @returns Command output
     */
    static async sh(command, options = {}, data = null) {
        // On Windows, try bash first (from Git for Windows), fall back to cmd.exe
        // On Unix-like systems, use sh
        let shell;
        let shellArgs;
        if (Config.OS === 'win32' || Config.OS === 'windows' || Config.OS === 'cygwin') {
            shell = Cmd.locate('bash');
            if (shell) {
                shellArgs = ['-c'];
            }
            else {
                shell = new Path('cmd.exe');
                shellArgs = ['/c'];
            }
        }
        else {
            shell = Cmd.locate('sh') || new Path('/bin/sh');
            shellArgs = ['-c'];
        }
        if (Array.isArray(command)) {
            // Quote arguments with spaces
            const quotedArgs = command.map(arg => {
                const s = String(arg).trimEnd();
                if (s.includes(' ') || s.includes('"') || s.includes("'")) {
                    return `'${s.replace(/'/g, "\\'")}'`;
                }
                return s;
            });
            const result = await Cmd.run([shell.name, ...shellArgs, quotedArgs.join(' ')], options, data);
            return result?.trimEnd() || '';
        }
        const result = await Cmd.run([shell.name, ...shellArgs, String(command).trimEnd()], options, data);
        return result?.trimEnd() || '';
    }
}
//# sourceMappingURL=Cmd.js.map