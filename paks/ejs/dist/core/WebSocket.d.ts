/**
 * WebSocket - WebSocket client
 *
 * Provides WebSocket networking functionality
 * @spec ejs
 * @stability evolving
 */
import { Emitter } from './async/Emitter';
export declare class WebSocket extends Emitter {
    private ws?;
    private _url;
    private _binaryType;
    private _protocols?;
    private _options?;
    private _frameProcessingPromise?;
    /**
     * Create a WebSocket client
     * @param url WebSocket URL (ws:// or wss://)
     * @param protocols Optional protocol or array of protocols
     * @param options Optional connection options
     */
    constructor(url: string, protocols?: string | string[], options?: any);
    /**
     * Connect to the WebSocket server
     */
    connect(): void;
    /**
     * Send data through the WebSocket
     * @param data Data to send
     */
    send(data: string | ArrayBuffer | Uint8Array): void;
    /**
     * Close the WebSocket connection
     * @param code Close code
     * @param reason Close reason
     */
    close(code?: number, reason?: string): void;
    /**
     * Get connection state
     */
    get readyState(): number;
    /**
     * Get buffered amount
     */
    get bufferedAmount(): number;
    /**
     * Get protocol
     */
    get protocol(): string;
    /**
     * Get URL
     */
    get url(): string;
    /**
     * Binary type for received binary data
     */
    get binaryType(): 'blob' | 'arraybuffer';
    set binaryType(type: 'blob' | 'arraybuffer');
    /**
     * Get negotiated extensions
     */
    get extensions(): string;
    /**
     * Set onopen handler
     */
    set onopen(handler: ((event: any) => void) | null);
    /**
     * Set onmessage handler
     */
    set onmessage(handler: ((event: any) => void) | null);
    /**
     * Set onerror handler
     */
    set onerror(handler: ((event: any) => void) | null);
    /**
     * Set onclose handler
     */
    set onclose(handler: ((event: any) => void) | null);
    /**
     * Send binary block data
     * @param data Binary data to send
     */
    sendBlock(data: ArrayBuffer | Uint8Array): void;
    /**
     * Wait for WebSocket to reach a specific state
     * Note: This is synchronous in native Ejscript but requires top-level await in Bun
     * Tests must be run with top-level await support or wrap in async function
     * @param state Target ready state (default: OPEN)
     * @param timeout Timeout in milliseconds (default: 30000)
     * @returns True if reached target state, false if timed out
     */
    wait(state?: number, timeout?: number): Promise<boolean>;
    static readonly CONNECTING = 0;
    static readonly OPEN = 1;
    static readonly CLOSING = 2;
    static readonly CLOSED = 3;
}
//# sourceMappingURL=WebSocket.d.ts.map