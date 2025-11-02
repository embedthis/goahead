/**
 * TextStream - Text-based stream I/O
 *
 * Wraps a binary stream to provide text-based reading and writing
 * @spec ejs
 * @stability evolving
 */
import { Stream } from './Stream';
export declare class TextStream extends Stream {
    private stream;
    private encoding;
    private _async;
    /**
     * Create a TextStream wrapping another stream
     * @param stream Underlying stream to wrap
     * @param encoding Text encoding (default 'utf-8')
     */
    constructor(stream: Stream, encoding?: string);
    get async(): boolean;
    set async(enable: boolean);
    close(): Promise<void>;
    flush(dir?: number): void;
    /**
     * Read data into a buffer
     * @param buffer Buffer to read into
     * @param offset Offset in buffer
     * @param count Number of bytes to read
     * @returns Number of bytes read, or null on EOF
     */
    read(buffer: Uint8Array, offset?: number, count?: number): Promise<number | null>;
    /**
     * Read a single line of text
     * @returns Line of text without newline, or null on EOF
     */
    readLine(): Promise<string | null>;
    /**
     * Read all lines from the stream
     * @returns Array of lines
     */
    readLines(): Promise<string[]>;
    /**
     * Read a string of specified length
     * @param count Number of characters to read (-1 for all)
     * @returns String read from stream, or null on EOF
     */
    readString(count?: number): Promise<string | null>;
    /**
     * Write data to the stream
     * @param ...args Data items to write (converted to strings)
     * @returns Number of bytes written
     */
    write(...args: any[]): Promise<number>;
    /**
     * Write lines of text (each arg becomes a separate line)
     * @param ...args Data items to write (each followed by newline)
     * @returns Number of bytes written
     */
    writeLine(...args: any[]): Promise<number>;
    on(name: string, observer: Function): this;
    off(name: string, observer: Function): void;
}
//# sourceMappingURL=TextStream.d.ts.map