/**
 * ByteArray - Resizable byte array for binary data
 *
 * Provides a growable byte buffer for binary I/O operations
 * @spec ejs
 * @stability evolving
 */
import * as zlib from 'zlib';
export class ByteArray extends Uint8Array {
    _readPosition = 0;
    _writePosition = 0;
    _growable;
    _size;
    _encoding = 'utf-8';
    _listeners = new Map();
    /**
     * Create a new ByteArray
     * @param size Initial size in bytes (default 1024)
     * @param growable Whether the array can grow automatically (default true)
     */
    constructor(size = 1024, growable = true) {
        super(size);
        this._growable = growable;
        this._size = size;
    }
    /**
     * Create a ByteArray from various sources
     * @param data Source data (string, array, buffer, etc.)
     * @param encoding Text encoding if data is a string
     * @returns New ByteArray containing the data
     */
    static fromData(data, _encoding) {
        if (typeof data === 'string') {
            const encoder = new TextEncoder();
            const encoded = encoder.encode(data);
            const ba = new ByteArray(encoded.length, false);
            ba.set(encoded);
            ba._writePosition = encoded.length;
            return ba;
        }
        else if (data instanceof ArrayBuffer) {
            const view = new Uint8Array(data);
            const ba = new ByteArray(view.length, false);
            ba.set(view);
            ba._writePosition = view.length;
            return ba;
        }
        else if (data instanceof ByteArray) {
            // Handle ByteArray - copy only the written data (0 to writePosition)
            const length = data.writePosition;
            const ba = new ByteArray(length, false);
            for (let i = 0; i < length; i++) {
                ba[i] = data[i];
            }
            ba._writePosition = length;
            return ba;
        }
        else if (Buffer.isBuffer(data)) {
            // Handle Node.js Buffer (must check before Uint8Array since Buffer extends Uint8Array)
            // Copy only actual bytes (not padding)
            const ba = new ByteArray(data.length, false);
            for (let i = 0; i < data.length; i++) {
                ba[i] = data[i];
            }
            ba._writePosition = data.length;
            return ba;
        }
        else if (data instanceof Uint8Array) {
            // Handle Uint8Array - copy all data
            const ba = new ByteArray(data.length, false);
            ba.set(data);
            ba._writePosition = data.length;
            return ba;
        }
        else {
            // Generic ArrayLike
            const length = data.length;
            const ba = new ByteArray(length, false);
            for (let i = 0; i < length; i++) {
                ba[i] = data[i];
            }
            ba._writePosition = length;
            return ba;
        }
    }
    /**
     * Total capacity of the buffer
     */
    get size() {
        return this._size;
    }
    /**
     * Amount of data written to the buffer (writePosition)
     * Note: Overriding length affects subarray() behavior - subarray results will have length=0
     * because their _writePosition is 0. Use the native Uint8Array length if you need the buffer size.
     */
    get length() {
        return this._writePosition;
    }
    /**
     * Available bytes remaining for reading (writePosition - readPosition)
     */
    get available() {
        return this._writePosition - this._readPosition;
    }
    /**
     * Current read position
     */
    get readPosition() {
        return this._readPosition;
    }
    set readPosition(pos) {
        if (pos < 0 || pos > this._writePosition) {
            throw new RangeError(`Read position ${pos} out of range [0, ${this._writePosition}]`);
        }
        this._readPosition = pos;
    }
    /**
     * Current write position
     */
    get writePosition() {
        return this._writePosition;
    }
    set writePosition(pos) {
        if (pos < 0 || pos > this._size) {
            throw new RangeError(`Write position ${pos} out of range [0, ${this._size}]`);
        }
        this._writePosition = pos;
    }
    /**
     * Room available for writing before needing to grow (size - writePosition)
     */
    get room() {
        return this._size - this._writePosition;
    }
    /**
     * Alias for room - bytes available for writing
     */
    get roomLeft() {
        return this.room;
    }
    /**
     * Alias for readPosition - current input position
     */
    get input() {
        return this._readPosition;
    }
    set input(pos) {
        this.readPosition = pos;
    }
    /**
     * Alias for writePosition - current output position
     */
    get output() {
        return this._writePosition;
    }
    set output(pos) {
        this.writePosition = pos;
    }
    /**
     * Whether the array can grow automatically
     */
    get resizable() {
        return this._growable;
    }
    set resizable(value) {
        this._growable = value;
    }
    /**
     * Character encoding for string operations
     */
    get encoding() {
        return this._encoding;
    }
    set encoding(value) {
        this._encoding = value;
    }
    /**
     * Compact the array by removing consumed bytes before readPosition
     * Shifts remaining data to the beginning
     */
    compact() {
        if (this._readPosition > 0) {
            const remaining = this._writePosition - this._readPosition;
            this.copyWithin(0, this._readPosition, this._writePosition);
            this._readPosition = 0;
            this._writePosition = remaining;
        }
    }
    /**
     * Flush the array (resets readPosition, emits readable event, then clears writePosition)
     */
    flush() {
        this._readPosition = 0;
        // Emit readable event first so handlers can read the data
        this._emit('readable', this);
        // Then reset writePosition to clear the buffer
        this._writePosition = 0;
    }
    /**
     * Compress the data using gzip compression
     * @returns New ByteArray containing compressed data
     */
    compress() {
        // Create a proper Buffer with only the data we want to compress
        const dataToCompress = Buffer.alloc(this._writePosition);
        for (let i = 0; i < this._writePosition; i++) {
            dataToCompress[i] = this[i];
        }
        // Compress using zlib (synchronous)
        const compressed = zlib.gzipSync(dataToCompress);
        // Use fromData to create properly initialized ByteArray
        return ByteArray.fromData(compressed);
    }
    /**
     * Uncompress the data using gzip decompression
     * @returns New ByteArray containing uncompressed data
     */
    uncompress() {
        // Create a proper Buffer with only the data we want to uncompress
        const dataToUncompress = Buffer.alloc(this._writePosition);
        for (let i = 0; i < this._writePosition; i++) {
            dataToUncompress[i] = this[i];
        }
        // Uncompress using zlib (synchronous)
        const uncompressed = zlib.gunzipSync(dataToUncompress);
        // Use fromData to create properly initialized ByteArray
        return ByteArray.fromData(uncompressed);
    }
    /**
     * Close the ByteArray (no-op for in-memory arrays)
     */
    close() {
        // No-op for in-memory ByteArray
    }
    /**
     * Reset the array (clears all data and positions)
     */
    reset() {
        this.fill(0);
        this._readPosition = 0;
        this._writePosition = 0;
    }
    /**
     * Grow the array to accommodate more data
     * @param amount Number of additional bytes needed
     * @returns New grown ByteArray
     */
    grow(amount) {
        if (!this._growable) {
            throw new Error('ByteArray is not growable');
        }
        const newSize = Math.max(this._size * 2, this._size + amount);
        const newArray = new ByteArray(newSize, this._growable);
        // Copy existing data
        newArray.set(this.subarray(0, this._size));
        // Copy positions and listeners
        newArray._readPosition = this._readPosition;
        newArray._writePosition = this._writePosition;
        newArray._listeners = this._listeners;
        return newArray;
    }
    /**
     * Read a single byte from the current read position
     * @returns The byte value (0-255), or null if no data available
     */
    readByte() {
        if (this._readPosition >= this._writePosition) {
            return null;
        }
        return this[this._readPosition++];
    }
    /**
     * Write a single byte at the current write position
     * @param value The byte value (0-255) to write
     */
    writeByte(value) {
        this._ensureRoom(1);
        this[this._writePosition++] = value & 0xff;
    }
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
    _ensureRoom(needed) {
        if (this.room >= needed)
            return;
        if (!this._growable) {
            throw new Error('ByteArray is not growable');
        }
        // Uint8Array is fixed-size - cannot truly grow after construction
        throw new Error(`ByteArray overflow: need ${needed} bytes but only ${this.room} available. ` +
            `ByteArray extends Uint8Array which has fixed size. ` +
            `Allocate sufficient initial size (current: ${this._size}, writePosition: ${this._writePosition})`);
    }
    /**
     * Read into a destination ByteArray from current read position
     * @param dest Destination ByteArray
     * @param offset Offset in destination (default 0)
     * @param count Number of bytes to read (default all available)
     * @returns Number of bytes read
     */
    read(dest, offset = 0, count = -1) {
        if (count < 0) {
            count = this.available;
        }
        count = Math.min(count, this.available);
        for (let i = 0; i < count; i++) {
            dest[offset + i] = this[this._readPosition++];
        }
        // Update destination's writePosition if we wrote beyond it
        if (offset + count > dest._writePosition) {
            dest._writePosition = offset + count;
        }
        return count;
    }
    /**
     * Read bytes from the current position
     * @param count Number of bytes to read (-1 for all available)
     * @returns New ByteArray containing the read data, or null if no data available
     */
    readBytes(count = -1) {
        if (this._readPosition >= this._writePosition) {
            return null;
        }
        if (count < 0) {
            count = this.available;
        }
        count = Math.min(count, this.available);
        const result = new ByteArray(count, false);
        for (let i = 0; i < count; i++) {
            result[i] = this[this._readPosition + i];
        }
        result._writePosition = count;
        this._readPosition += count;
        return result;
    }
    /**
     * Read a string from the current position
     * @param count Number of bytes to read (-1 for all available)
     * @param encoding Text encoding (uses this.encoding if not specified)
     * @returns Decoded string, or null if no data available
     */
    readString(count = -1, encoding) {
        if (count < 0) {
            count = this.available;
        }
        if (count === 0 || this._readPosition >= this._writePosition) {
            return null;
        }
        count = Math.min(count, this.available);
        const decoder = new TextDecoder((encoding || this._encoding));
        const view = new Uint8Array(this.buffer, this.byteOffset + this._readPosition, count);
        const result = decoder.decode(view);
        this._readPosition += count;
        return result;
    }
    /**
     * Write a string at the current position
     * @param str String to write
     * @param encoding Text encoding (uses this.encoding if not specified)
     * @returns Number of bytes written
     */
    writeString(str, encoding) {
        const encoder = new TextEncoder(); // Note: TextEncoder only supports utf-8
        const bytes = encoder.encode(str);
        this._ensureRoom(bytes.length);
        this.set(bytes, this._writePosition);
        this._writePosition += bytes.length;
        return bytes.length;
    }
    /**
     * Read a 16-bit short from the current position
     * @returns The short value, or null if insufficient data
     */
    readShort() {
        if (this.available < 2)
            return null;
        const view = new DataView(this.buffer, this._readPosition, 2);
        this._readPosition += 2;
        return view.getInt16(0, true); // little endian
    }
    /**
     * Write a 16-bit short at the current position
     * @param value The short value to write
     */
    writeShort(value) {
        if (this.room < 2) {
            this.grow(2);
        }
        const view = new DataView(this.buffer, this._writePosition, 2);
        view.setInt16(0, value, true); // little endian
        this._writePosition += 2;
    }
    /**
     * Read a 32-bit integer from the current position
     * @returns The integer value, or null if insufficient data
     */
    readInteger32() {
        if (this.available < 4)
            return null;
        const view = new DataView(this.buffer, this._readPosition, 4);
        this._readPosition += 4;
        return view.getInt32(0, true); // little endian
    }
    /**
     * Write a 32-bit integer at the current position
     * @param value The integer value to write
     */
    writeInteger32(value) {
        if (this.room < 4) {
            this.grow(4);
        }
        const view = new DataView(this.buffer, this._writePosition, 4);
        view.setInt32(0, value, true); // little endian
        this._writePosition += 4;
    }
    /**
     * Read a 64-bit long from the current position
     * @returns The long value as BigInt, or null if insufficient data
     */
    readLong() {
        if (this.available < 8)
            return null;
        const view = new DataView(this.buffer, this._readPosition, 8);
        this._readPosition += 8;
        return view.getBigInt64(0, true); // little endian
    }
    /**
     * Write a 64-bit long at the current position
     * @param value The long value (BigInt) to write
     */
    writeLong(value) {
        if (this.room < 8) {
            this.grow(8);
        }
        const view = new DataView(this.buffer, this._writePosition, 8);
        view.setBigInt64(0, value, true); // little endian
        this._writePosition += 8;
    }
    /**
     * Read a 64-bit double from the current position
     * @returns The double value, or null if insufficient data
     */
    readDouble() {
        if (this.available < 8)
            return null;
        const view = new DataView(this.buffer, this._readPosition, 8);
        this._readPosition += 8;
        return view.getFloat64(0, true); // little endian
    }
    /**
     * Write a 64-bit double at the current position
     * @param value The double value to write
     */
    writeDouble(value) {
        if (this.room < 8) {
            this.grow(8);
        }
        const view = new DataView(this.buffer, this._writePosition, 8);
        view.setFloat64(0, value, true); // little endian
        this._writePosition += 8;
    }
    /**
     * Write bytes or string to the current write position
     * @param data Bytes or string to write
     * @returns Number of bytes written
     */
    write(data) {
        if (typeof data === 'string') {
            const encoder = new TextEncoder();
            const bytes = encoder.encode(data);
            this._ensureRoom(bytes.length);
            this.set(bytes, this._writePosition);
            this._writePosition += bytes.length;
            return bytes.length;
        }
        else {
            this._ensureRoom(data.length);
            this.set(data, this._writePosition);
            this._writePosition += data.length;
            return data.length;
        }
    }
    /**
     * Write data to the current position
     * @param data Data to write (ByteArray, Uint8Array, string, number)
     * @param encoding Text encoding if data is a string
     * @returns Number of bytes written
     */
    writeData(data) {
        let bytes;
        if (typeof data === 'string') {
            const encoder = new TextEncoder();
            bytes = encoder.encode(data);
        }
        else if (typeof data === 'number') {
            bytes = new Uint8Array([data]);
        }
        else {
            bytes = data;
        }
        return this.write(bytes);
    }
    /**
     * Copy data from another ByteArray into this one
     * @param destOffset Offset in this ByteArray where data will be written
     * @param src Source ByteArray
     * @param srcOffset Offset in source (default 0)
     * @param count Number of bytes to copy (default all available in source)
     * @returns Number of bytes copied
     */
    copyIn(destOffset, src, srcOffset = 0, count = -1) {
        if (count < 0) {
            count = src.length - srcOffset;
        }
        count = Math.min(count, src.length - srcOffset);
        for (let i = 0; i < count; i++) {
            this[destOffset + i] = src[srcOffset + i];
        }
        return count;
    }
    /**
     * Copy data from this ByteArray to another
     * @param srcOffset Offset in this ByteArray to copy from
     * @param dest Destination ByteArray
     * @param destOffset Offset in destination (default 0)
     * @param count Number of bytes to copy (default all remaining)
     * @returns Number of bytes copied
     */
    copyOut(srcOffset, dest, destOffset = 0, count = -1) {
        if (count < 0) {
            count = this.length - srcOffset;
        }
        count = Math.min(count, this.length - srcOffset);
        for (let i = 0; i < count; i++) {
            dest[destOffset + i] = this[srcOffset + i];
        }
        return count;
    }
    /**
     * Register an event listener
     * @param event Event name ('readable' or 'writable')
     * @param callback Callback function
     */
    on(event, callback) {
        if (!this._listeners.has(event)) {
            this._listeners.set(event, []);
        }
        this._listeners.get(event).push(callback);
        // Fire writable immediately if registering writable event
        if (event === 'writable') {
            callback(event, this);
        }
        return this;
    }
    /**
     * Unregister an event listener
     * @param event Event name
     * @param callback Callback function to remove
     */
    off(event, callback) {
        const listeners = this._listeners.get(event);
        if (listeners) {
            const index = listeners.indexOf(callback);
            if (index > -1) {
                listeners.splice(index, 1);
            }
        }
        return this;
    }
    /**
     * Emit an event to all registered listeners
     * @param event Event name
     * @param args Arguments to pass to listeners
     */
    _emit(event, ...args) {
        const listeners = this._listeners.get(event);
        if (listeners) {
            for (const callback of listeners) {
                callback(event, ...args);
            }
        }
    }
    /**
     * Convert to a string (returns available data from readPosition to writePosition)
     * @param encoding Text encoding (default 'utf-8')
     * @returns Decoded string
     */
    toString() {
        const decoder = new TextDecoder('utf-8');
        // Create a proper Uint8Array view for available data
        const length = this._writePosition - this._readPosition;
        const view = new Uint8Array(this.buffer, this.byteOffset + this._readPosition, length);
        return decoder.decode(view);
    }
    /**
     * Convert to a regular Uint8Array
     * @returns Uint8Array containing the data up to writePosition
     */
    toArray() {
        const result = new Uint8Array(this._writePosition);
        for (let i = 0; i < this._writePosition; i++) {
            result[i] = this[i];
        }
        return result;
    }
    /**
     * Iterator support - iterates over available data from readPosition to writePosition
     * For subarray results (where _writePosition would be 0), falls back to iterating the full buffer
     */
    *[Symbol.iterator]() {
        // Determine the range to iterate
        // If this looks like a properly initialized ByteArray with writePosition > 0, use that range
        // Otherwise (e.g., subarray results), iterate from readPosition to buffer end
        const start = this._readPosition;
        const end = this._writePosition > 0 ? this._writePosition : this.byteLength;
        for (let i = start; i < end; i++) {
            yield this[i];
        }
    }
}
//# sourceMappingURL=ByteArray.js.map