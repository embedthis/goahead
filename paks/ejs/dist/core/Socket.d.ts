/**
 * Socket - TCP/UDP socket support
 *
 * Provides socket networking functionality
 * @spec ejs
 * @stability evolving
 */
/**
 * Socket class for TCP/UDP networking
 * Note: Socket does NOT extend Stream because network I/O is async in JavaScript
 */
export declare class Socket {
    private tcpSocket?;
    private udpSocket?;
    private emitter;
    private _isUdp;
    private _encoding;
    private _isEof;
    private _server?;
    private _localPort;
    private _localAddress;
    private _acceptQueue;
    private _options;
    private _dataBuffer;
    /**
     * Create a socket
     * @param options Socket options
     */
    constructor(options?: any);
    /**
     * Async mode (deprecated - sockets are event-driven)
     * @deprecated Socket operations are event-driven by default
     */
    get async(): boolean;
    set async(_enable: boolean);
    /**
     * Socket options
     */
    get options(): any;
    set options(opts: any);
    /**
     * Character encoding for serializing strings
     */
    get encoding(): string;
    set encoding(enc: string);
    /**
     * Check if socket is at end of input
     */
    get isEof(): boolean;
    /**
     * Local port number bound to this socket
     */
    get port(): number;
    /**
     * Connect to a remote host (TCP)
     * @param address Port number, IP string, "IP:PORT" string, or port number string
     * @throws Error if connection fails
     */
    connect(address: number | string): void;
    /**
     * Bind socket to address (UDP)
     * @param address Address to bind to
     * @param port Port to bind to
     */
    bind(address: string, port: number): void;
    /**
     * Listen for connections (TCP server)
     * @param address Port number, IP string, "IP:PORT" string, or port number string
     * @param backlog Connection backlog
     */
    listen(address: number | string, backlog?: number): void;
    /**
     * Accept an incoming connection
     * @returns New Socket for the accepted connection
     */
    accept(): Socket;
    close(): void;
    flush(_dir?: number): void;
    read(buffer: Uint8Array, offset?: number, count?: number): Promise<number | null>;
    write(...data: any[]): Promise<number>;
    on(name: string, observer: Function): this;
    off(name: string, observer: Function): void;
    /**
     * Get local address
     */
    get address(): {
        address: string;
        port: number;
    } | null;
    /**
     * Get remote address
     */
    get remoteAddress(): {
        address: string;
        port: number;
    } | null;
}
//# sourceMappingURL=Socket.d.ts.map