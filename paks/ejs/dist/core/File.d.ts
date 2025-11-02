/**
 * File - File I/O class
 *
 * Provides file I/O services to interact with physical files
 * @spec ejs
 * @stability evolving
 */
import { Stream } from './streams/Stream';
import { Path } from './Path';
import { ByteArray } from './streams/ByteArray';
export interface FileOptions {
    mode?: string;
    permissions?: number;
    owner?: string;
    group?: string;
}
export declare class File extends Stream {
    private _path;
    private _handle;
    private _mode;
    private _permissions;
    private _position;
    private _async;
    private _canRead;
    private _canWrite;
    private _encoding;
    private observers;
    /**
     * Create a File object
     * @param path Path to the file
     * @param options Open options (stored but file is NOT opened until open() is called)
     * Note: With async open(), constructor no longer auto-opens the file.
     * Call await file.open() explicitly.
     */
    constructor(path: string | Path, options?: FileOptions | string);
    get async(): boolean;
    set async(enable: boolean);
    /**
     * Can the file be read
     */
    get canRead(): boolean;
    /**
     * Can the file be written
     */
    get canWrite(): boolean;
    /**
     * Close the file
     */
    close(): Promise<void>;
    /**
     * Text encoding for string operations
     * Supported encodings: 'utf-8', 'utf8', 'ascii', 'latin1', 'base64', 'hex'
     */
    get encoding(): string;
    set encoding(enc: string);
    /**
     * Flush data (no-op for unbuffered I/O)
     */
    flush(_dir?: number): void;
    /**
     * Is the file open
     */
    get isOpen(): boolean;
    /**
     * Open the file
     * @param options Open options
     * @returns This File object for chaining
     */
    open(options?: FileOptions | string | null): Promise<File>;
    /**
     * Get current file options
     */
    get options(): FileOptions;
    /**
     * Get the file path
     */
    get path(): Path;
    /**
     * Get/set current read/write position
     */
    get position(): number;
    set position(loc: number);
    /**
     * Read data from file into a buffer
     * @param buffer Destination byte array
     * @param offset Offset in buffer to write data
     * @param count Number of bytes to read (-1 for all)
     * @returns Number of bytes read, or null on EOF
     */
    read(buffer: Uint8Array, offset?: number, count?: number): Promise<number | null>;
    /**
     * Read bytes from file
     * @param count Number of bytes to read (-1 for entire file)
     * @returns ByteArray with data, or null on EOF
     */
    readBytes(count?: number): Promise<ByteArray | null>;
    /**
     * Read string from file
     * @param count Number of bytes to read (-1 for entire file)
     * @returns String data, or null on EOF
     */
    readString(count?: number): Promise<string | null>;
    /**
     * Read file as array of lines
     * @returns Array of lines, or null on error
     */
    readLines(): Promise<string[] | null>;
    /**
     * Seek to a specific position in the file
     * @param loc Position to seek to
     * @param whence Reference point (0=start, 1=current, 2=end)
     */
    seek(loc: number, whence?: number): void;
    /**
     * Remove the file
     * @returns True if successful
     */
    remove(): Promise<boolean>;
    /**
     * Get file size in bytes
     */
    get size(): number;
    /**
     * Truncate the file
     * @param value New file size
     */
    truncate(value: number): void;
    /**
     * Write data to file
     * @param ...items Data items to write
     * @returns Number of bytes written
     */
    write(...items: any[]): Promise<number>;
    /**
     * Write line to file (with newline)
     * @param ...items Data items to write
     * @returns Number of bytes written
     */
    writeLine(...items: any[]): Promise<number>;
    /**
     * Iterator - yields file positions
     */
    [Symbol.iterator](): Iterator<number>;
    on(name: string, observer: Function): this;
    off(name: string, observer: Function): void;
    /**
     * Open a text stream for reading or writing
     * @param mode Mode: 'r', 'w', 'a', 'rt', 'wt', 'at'
     * @returns TextStream instance
     */
    openTextStream(mode?: string): Promise<any>;
    /**
     * Open a binary stream for reading or writing
     * @param mode Mode: 'r', 'w', 'a'
     * @returns BinaryStream instance
     */
    openBinaryStream(mode?: string): Promise<any>;
    private _emit;
}
//# sourceMappingURL=File.d.ts.map