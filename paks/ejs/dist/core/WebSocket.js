/**
 * WebSocket - WebSocket client
 *
 * Provides WebSocket networking functionality
 * @spec ejs
 * @stability evolving
 */
import { Emitter } from './async/Emitter';
export class WebSocket extends Emitter {
    ws;
    _url;
    _binaryType = 'blob';
    _protocols;
    _options;
    _frameProcessingPromise; // Track async frame processing
    /**
     * Create a WebSocket client
     * @param url WebSocket URL (ws:// or wss://)
     * @param protocols Optional protocol or array of protocols
     * @param options Optional connection options
     */
    constructor(url, protocols, options) {
        super();
        this._url = url;
        this._protocols = protocols;
        this._options = options;
        // Auto-connect like native Ejscript
        this.connect();
    }
    /**
     * Connect to the WebSocket server
     */
    connect() {
        // Support Bun's WebSocket options (verify: false for self-signed certs)
        const wsOptions = {};
        if (this._options?.verify === false) {
            wsOptions.tls = { rejectUnauthorized: false };
        }
        // Create WebSocket with optional protocols
        if (this._protocols) {
            this.ws = new globalThis.WebSocket(this._url, this._protocols);
        }
        else {
            this.ws = new globalThis.WebSocket(this._url);
        }
        // Apply TLS options if needed (Bun specific)
        if (wsOptions.tls && this.ws.tls) {
            Object.assign(this.ws.tls, wsOptions.tls);
        }
        // Bun's WebSocket has 'nodebuffer' instead of 'blob'
        if (this._binaryType === 'blob') {
            this.ws.binaryType = 'nodebuffer';
        }
        else {
            this.ws.binaryType = this._binaryType;
        }
        // Set up internal handlers that emit events AND call user handlers
        this.ws.onopen = (event) => {
            this.emit('statechange'); // Emit state change for wait()
            this.emit('open', event);
            // Also call user's onopen if set
            if (this._userOnOpen) {
                this._userOnOpen(event);
            }
        };
        this.ws.onmessage = (event) => {
            // Handle frames mode - split message into individual frames
            if (this._options?.frames && typeof event.data === 'string') {
                // In frames mode, split the assembled message into individual frames
                // The server sends each frame with HTTP_MORE flag, which marks them as continuation frames
                // Bun auto-assembles them into one message, so we need to split them back
                // Each frame ends with newline in the test server implementation
                const frames = event.data.split('\n');
                // Process frames asynchronously to avoid blocking event loop
                const processFrames = async () => {
                    for (let i = 0; i < frames.length; i++) {
                        const frame = frames[i];
                        if (frame.length === 0)
                            continue; // Skip empty frames from trailing newline
                        // Add back the newline that was removed by split
                        const frameData = frame + '\n';
                        const isLast = (i === frames.length - 2); // Second to last (last is empty after final \n)
                        const messageEvent = {
                            data: frameData,
                            type: 1, // 1=text frame
                            last: isLast,
                            target: this
                        };
                        this.emit('message', messageEvent);
                        if (this._userOnMessage) {
                            this._userOnMessage(messageEvent);
                        }
                        // Yield to event loop every 100 frames
                        if (i % 100 === 0) {
                            await Bun.sleep(0);
                        }
                    }
                };
                // Start processing frames asynchronously and track the promise
                this._frameProcessingPromise = processFrames();
            }
            else {
                // Normal mode - deliver complete assembled message
                const messageEvent = {
                    data: event.data,
                    type: typeof event.data === 'string' ? 1 : 2, // 1=text, 2=binary
                    last: true, // Bun's WebSocket auto-assembles fragments, so always true
                    target: this
                };
                this.emit('message', messageEvent);
                // Also call user's onmessage if set
                if (this._userOnMessage) {
                    this._userOnMessage(messageEvent);
                }
            }
        };
        this.ws.onerror = (event) => {
            this.emit('error', event);
            // Also call user's onerror if set
            if (this._userOnError) {
                this._userOnError(event);
            }
        };
        this.ws.onclose = async (event) => {
            // Wait for frame processing to complete before closing
            if (this._frameProcessingPromise) {
                await this._frameProcessingPromise;
                this._frameProcessingPromise = undefined;
            }
            this.emit('statechange'); // Emit state change for wait()
            this.emit('close', event);
            // Also call user's onclose if set
            if (this._userOnClose) {
                this._userOnClose(event);
            }
        };
    }
    /**
     * Send data through the WebSocket
     * @param data Data to send
     */
    send(data) {
        if (!this.ws) {
            throw new Error('WebSocket not connected');
        }
        this.ws.send(data);
    }
    /**
     * Close the WebSocket connection
     * @param code Close code
     * @param reason Close reason
     */
    close(code, reason) {
        if (this.ws) {
            this.ws.close(code, reason);
        }
    }
    /**
     * Get connection state
     */
    get readyState() {
        return this.ws?.readyState ?? 0;
    }
    /**
     * Get buffered amount
     */
    get bufferedAmount() {
        return this.ws?.bufferedAmount ?? 0;
    }
    /**
     * Get protocol
     */
    get protocol() {
        return this.ws?.protocol ?? '';
    }
    /**
     * Get URL
     */
    get url() {
        return this._url;
    }
    /**
     * Binary type for received binary data
     */
    get binaryType() {
        return this._binaryType;
    }
    set binaryType(type) {
        this._binaryType = type;
        if (this.ws) {
            // Bun's WebSocket has 'nodebuffer' instead of 'blob'
            if (type === 'blob') {
                this.ws.binaryType = 'nodebuffer';
            }
            else {
                this.ws.binaryType = type;
            }
        }
    }
    /**
     * Get negotiated extensions
     */
    get extensions() {
        return this.ws?.extensions ?? '';
    }
    /**
     * Set onopen handler
     */
    set onopen(handler) {
        this._userOnOpen = handler;
    }
    /**
     * Set onmessage handler
     */
    set onmessage(handler) {
        this._userOnMessage = handler;
    }
    /**
     * Set onerror handler
     */
    set onerror(handler) {
        this._userOnError = handler;
    }
    /**
     * Set onclose handler
     */
    set onclose(handler) {
        this._userOnClose = handler;
    }
    /**
     * Send binary block data
     * @param data Binary data to send
     */
    sendBlock(data) {
        this.send(data);
    }
    /**
     * Wait for WebSocket to reach a specific state
     * Note: This is synchronous in native Ejscript but requires top-level await in Bun
     * Tests must be run with top-level await support or wrap in async function
     * @param state Target ready state (default: OPEN)
     * @param timeout Timeout in milliseconds (default: 30000)
     * @returns True if reached target state, false if timed out
     */
    async wait(state = WebSocket.OPEN, timeout = 30000) {
        // Helper to check if we've reached the desired state
        const isInDesiredState = () => {
            if (this.readyState === state) {
                return true;
            }
            // Bun WebSocket quirk: When closing during CONNECTING, it stays in CLOSING
            // and never fires onclose. Treat CLOSING as CLOSED for wait(CLOSED).
            if (state === WebSocket.CLOSED && this.readyState === WebSocket.CLOSING) {
                return true;
            }
            return false;
        };
        // Check current state immediately
        if (isInDesiredState()) {
            return true;
        }
        return new Promise((resolve) => {
            let timeoutId;
            const cleanup = () => {
                if (timeoutId)
                    clearTimeout(timeoutId);
                this.off('statechange', onStateChange);
            };
            // Set up timeout
            timeoutId = setTimeout(() => {
                cleanup();
                resolve(false);
            }, timeout);
            // Listen for state changes via internal event
            const onStateChange = () => {
                if (isInDesiredState()) {
                    cleanup();
                    resolve(true);
                }
            };
            // Use internal event listener for state changes
            this.on('statechange', onStateChange);
            // Also check immediately in case state changed between initial check and listener setup
            if (isInDesiredState()) {
                cleanup();
                resolve(true);
            }
        });
    }
    // WebSocket ready states
    static CONNECTING = 0;
    static OPEN = 1;
    static CLOSING = 2;
    static CLOSED = 3;
}
//# sourceMappingURL=WebSocket.js.map