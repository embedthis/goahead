/**
 * ByteArray - Resizable byte array for binary data
 *
 * Provides a growable byte buffer for binary I/O operations
 * @spec ejs
 * @stability evolving
 */
export declare class ByteArray extends Uint8Array {
    private _readPosition;
    private _writePosition;
    private _growable;
    private _size;
    private _encoding;
    private _listeners;
    /**
     * Create a new ByteArray
     * @param size Initial size in bytes (default 1024)
     * @param growable Whether the array can grow automatically (default true)
     */
    constructor(size?: number, growable?: boolean);
    /**
     * Create a ByteArray from various sources
     * @param data Source data (string, array, buffer, etc.)
     * @param encoding Text encoding if data is a string
     * @returns New ByteArray containing the data
     */
    static fromData(data: string | ArrayLike<number> | ArrayBufferLike, _encoding?: string): ByteArray;
    /**
     * Total capacity of the buffer
     */
    get size(): number;
    /**
     * Amount of data written to the buffer (writePosition)
     * Note: Overriding length affects subarray() behavior - subarray results will have length=0
     * because their _writePosition is 0. Use the native Uint8Array length if you need the buffer size.
     */
    get length(): number;
    /**
     * Available bytes remaining for reading (writePosition - readPosition)
     */
    get available(): number;
    /**
     * Current read position
     */
    get readPosition(): number;
    set readPosition(pos: number);
    /**
     * Current write position
     */
    get writePosition(): number;
    set writePosition(pos: number);
    /**
     * Room available for writing before needing to grow (size - writePosition)
     */
    get room(): number;
    /**
     * Alias for room - bytes available for writing
     */
    get roomLeft(): number;
    /**
     * Alias for readPosition - current input position
     */
    get input(): number;
    set input(pos: number);
    /**
     * Alias for writePosition - current output position
     */
    get output(): number;
    set output(pos: number);
    /**
     * Whether the array can grow automatically
     */
    get resizable(): boolean;
    set resizable(value: boolean);
    /**
     * Character encoding for string operations
     */
    get encoding(): string;
    set encoding(value: string);
    /**
     * Compact the array by removing consumed bytes before readPosition
     * Shifts remaining data to the beginning
     */
    compact(): void;
    /**
     * Flush the array (resets readPosition, emits readable event, then clears writePosition)
     */
    flush(): void;
    /**
     * Compress the data using gzip compression
     * @returns New ByteArray containing compressed data
     */
    compress(): ByteArray;
    /**
     * Uncompress the data using gzip decompression
     * @returns New ByteArray containing uncompressed data
     */
    uncompress(): ByteArray;
    /**
     * Close the ByteArray (no-op for in-memory arrays)
     */
    close(): void;
    /**
     * Reset the array (clears all data and positions)
     */
    reset(): void;
    /**
     * Grow the array to accommodate more data
     * @param amount Number of additional bytes needed
     * @returns New grown ByteArray
     */
    private grow;
    /**
     * Read a single byte from the current read position
     * @returns The byte value (0-255), or null if no data available
     */
    readByte(): number | null;
    /**
     * Write a single byte at the current write position
     * @param value The byte value (0-255) to write
     */
    writeByte(value: number): void;
    /**
     * Ensure there is enough room for writing
     * @param needed Number of bytes needed
     *
     * Note: ByteArray extends Uint8Array which has fixed size after construction.
     * True dynamic growth is not possible without a major refactor to use composition
     * instead of inheritance. For now, allocate sufficient initial size.
     *
     * Workaround: Create ByteArray with large initial size (e.g., new ByteArray(1024 * 1024))
     */
    private _ensureRoom;
    /**
     * Read into a destination ByteArray from current read position
     * @param dest Destination ByteArray
     * @param offset Offset in destination (default 0)
     * @param count Number of bytes to read (default all available)
     * @returns Number of bytes read
     */
    read(dest: ByteArray, offset?: number, count?: number): number;
    /**
     * Read bytes from the current position
     * @param count Number of bytes to read (-1 for all available)
     * @returns New ByteArray containing the read data, or null if no data available
     */
    readBytes(count?: number): ByteArray | null;
    /**
     * Read a string from the current position
     * @param count Number of bytes to read (-1 for all available)
     * @param encoding Text encoding (uses this.encoding if not specified)
     * @returns Decoded string, or null if no data available
     */
    readString(count?: number, encoding?: string): string | null;
    /**
     * Write a string at the current position
     * @param str String to write
     * @param encoding Text encoding (uses this.encoding if not specified)
     * @returns Number of bytes written
     */
    writeString(str: string, encoding?: string): number;
    /**
     * Read a 16-bit short from the current position
     * @returns The short value, or null if insufficient data
     */
    readShort(): number | null;
    /**
     * Write a 16-bit short at the current position
     * @param value The short value to write
     */
    writeShort(value: number): void;
    /**
     * Read a 32-bit integer from the current position
     * @returns The integer value, or null if insufficient data
     */
    readInteger32(): number | null;
    /**
     * Write a 32-bit integer at the current position
     * @param value The integer value to write
     */
    writeInteger32(value: number): void;
    /**
     * Read a 64-bit long from the current position
     * @returns The long value as BigInt, or null if insufficient data
     */
    readLong(): bigint | null;
    /**
     * Write a 64-bit long at the current position
     * @param value The long value (BigInt) to write
     */
    writeLong(value: bigint): void;
    /**
     * Read a 64-bit double from the current position
     * @returns The double value, or null if insufficient data
     */
    readDouble(): number | null;
    /**
     * Write a 64-bit double at the current position
     * @param value The double value to write
     */
    writeDouble(value: number): void;
    /**
     * Write bytes or string to the current write position
     * @param data Bytes or string to write
     * @returns Number of bytes written
     */
    write(data: Uint8Array | string): number;
    /**
     * Write data to the current position
     * @param data Data to write (ByteArray, Uint8Array, string, number)
     * @param encoding Text encoding if data is a string
     * @returns Number of bytes written
     */
    writeData(data: ByteArray | Uint8Array | string | number): number;
    /**
     * Copy data from another ByteArray into this one
     * @param destOffset Offset in this ByteArray where data will be written
     * @param src Source ByteArray
     * @param srcOffset Offset in source (default 0)
     * @param count Number of bytes to copy (default all available in source)
     * @returns Number of bytes copied
     */
    copyIn(destOffset: number, src: ByteArray, srcOffset?: number, count?: number): number;
    /**
     * Copy data from this ByteArray to another
     * @param srcOffset Offset in this ByteArray to copy from
     * @param dest Destination ByteArray
     * @param destOffset Offset in destination (default 0)
     * @param count Number of bytes to copy (default all remaining)
     * @returns Number of bytes copied
     */
    copyOut(srcOffset: number, dest: ByteArray, destOffset?: number, count?: number): number;
    /**
     * Register an event listener
     * @param event Event name ('readable' or 'writable')
     * @param callback Callback function
     */
    on(event: string, callback: Function): this;
    /**
     * Unregister an event listener
     * @param event Event name
     * @param callback Callback function to remove
     */
    off(event: string, callback: Function): this;
    /**
     * Emit an event to all registered listeners
     * @param event Event name
     * @param args Arguments to pass to listeners
     */
    private _emit;
    /**
     * Convert to a string (returns available data from readPosition to writePosition)
     * @param encoding Text encoding (default 'utf-8')
     * @returns Decoded string
     */
    toString(): string;
    /**
     * Convert to a regular Uint8Array
     * @returns Uint8Array containing the data up to writePosition
     */
    toArray(): Uint8Array;
    /**
     * Iterator support - iterates over available data from readPosition to writePosition
     * For subarray results (where _writePosition would be 0), falls back to iterating the full buffer
     */
    [Symbol.iterator](): ArrayIterator<number>;
}
//# sourceMappingURL=ByteArray.d.ts.map