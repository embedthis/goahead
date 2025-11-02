/**
 * Socket - TCP/UDP socket support
 *
 * Provides socket networking functionality
 * @spec ejs
 * @stability evolving
 */
import { Emitter } from './async/Emitter';
import * as net from 'net';
import * as dgram from 'dgram';
/**
 * Socket class for TCP/UDP networking
 * Note: Socket does NOT extend Stream because network I/O is async in JavaScript
 */
export class Socket {
    tcpSocket;
    udpSocket;
    emitter = new Emitter();
    _isUdp = false;
    _encoding = 'utf-8';
    _isEof = false;
    _server;
    _localPort = 0;
    _localAddress = '';
    _acceptQueue = [];
    _options;
    _dataBuffer = [];
    /**
     * Create a socket
     * @param options Socket options
     */
    constructor(options) {
        this._options = options || {};
        if (options?.datagram) {
            this._isUdp = true;
        }
    }
    /**
     * Async mode (deprecated - sockets are event-driven)
     * @deprecated Socket operations are event-driven by default
     */
    get async() {
        return true;
    }
    set async(_enable) {
        // No-op: socket operations are event-driven
    }
    /**
     * Socket options
     */
    get options() {
        return this._options;
    }
    set options(opts) {
        this._options = opts;
        if (opts.datagram) {
            this._isUdp = true;
        }
    }
    /**
     * Character encoding for serializing strings
     */
    get encoding() {
        return this._encoding;
    }
    set encoding(enc) {
        this._encoding = enc;
    }
    /**
     * Check if socket is at end of input
     */
    get isEof() {
        return this._isEof;
    }
    /**
     * Local port number bound to this socket
     */
    get port() {
        return this._localPort;
    }
    /**
     * Connect to a remote host (TCP)
     * @param address Port number, IP string, "IP:PORT" string, or port number string
     * @throws Error if connection fails
     */
    connect(address) {
        let host = 'localhost';
        let port;
        if (typeof address === 'number') {
            port = address;
            if (port < 0 || port > 65535) {
                throw new Error('Invalid port number');
            }
        }
        else if (typeof address === 'string') {
            // Strip protocol if present (http:// or https://)
            let addressStr = address.replace(/^https?:\/\//, '');
            // Check for host:port format
            if (addressStr.includes(':')) {
                const parts = addressStr.split(':');
                if (parts.length !== 2) {
                    throw new Error('Invalid address format');
                }
                host = parts[0];
                port = parseInt(parts[1], 10);
            }
            else {
                port = parseInt(addressStr, 10);
            }
            if (isNaN(port) || port < 0 || port > 65535) {
                throw new Error('Invalid port number');
            }
        }
        else {
            throw new Error('Invalid address format');
        }
        this.tcpSocket = net.connect(port, host);
        this.tcpSocket.on('data', (data) => {
            this._dataBuffer.push(Buffer.from(data));
            this.emitter.emit('readable', data);
        });
        this.tcpSocket.on('error', (error) => {
            this.emitter.emit('error', error);
            // Don't throw here - let the error be handled by the event system
        });
        this.tcpSocket.on('close', () => {
            this._isEof = true;
            this.emitter.emit('close', this);
        });
        this.tcpSocket.on('end', () => {
            this._isEof = true;
        });
        this.tcpSocket.on('connect', () => {
            this.emitter.emit('writable', this);
        });
    }
    /**
     * Bind socket to address (UDP)
     * @param address Address to bind to
     * @param port Port to bind to
     */
    bind(address, port) {
        this._isUdp = true;
        this.udpSocket = dgram.createSocket('udp4');
        this.udpSocket.on('message', (msg, _rinfo) => {
            this.emitter.emit('readable', msg);
        });
        this.udpSocket.on('error', (error) => {
            this.emitter.emit('error', error);
        });
        this.udpSocket.bind(port, address);
    }
    /**
     * Listen for connections (TCP server)
     * @param address Port number, IP string, "IP:PORT" string, or port number string
     * @param backlog Connection backlog
     */
    listen(address, backlog) {
        let host = undefined;
        let port;
        if (typeof address === 'number') {
            port = address;
            if (port < 0 || port > 65535) {
                throw new Error('Invalid port number');
            }
        }
        else if (typeof address === 'string') {
            if (address.includes(':')) {
                const parts = address.split(':');
                host = parts[0];
                port = parseInt(parts[1], 10);
            }
            else {
                port = parseInt(address, 10);
            }
            if (isNaN(port) || port < 0 || port > 65535) {
                throw new Error('Invalid port number');
            }
        }
        else {
            throw new Error('Invalid address format');
        }
        this._server = net.createServer((socket) => {
            // Always emit events (event-driven mode)
            this.emitter.emit('accept', socket);
            // Also queue for accept() method compatibility
            this._acceptQueue.push(socket);
        });
        // Use listenSync-like behavior by immediately setting port
        // The actual binding happens asynchronously
        this._localPort = port;
        if (host) {
            this._localAddress = host;
        }
        this._server.listen(port, host, backlog);
        this._server.on('listening', () => {
            const addr = this._server?.address();
            if (addr && typeof addr === 'object') {
                this._localPort = addr.port;
                this._localAddress = addr.address;
            }
        });
        this._server.on('error', (error) => {
            this.emitter.emit('error', error);
        });
    }
    /**
     * Accept an incoming connection
     * @returns New Socket for the accepted connection
     */
    accept() {
        // In sync mode, wait for a connection from the queue
        if (this._acceptQueue.length > 0) {
            const socket = this._acceptQueue.shift();
            const newSocket = new Socket();
            newSocket.tcpSocket = socket;
            socket.on('data', (data) => {
                newSocket._dataBuffer.push(Buffer.from(data));
                newSocket.emitter.emit('readable', data);
            });
            socket.on('error', (error) => {
                newSocket.emitter.emit('error', error);
            });
            socket.on('close', () => {
                newSocket._isEof = true;
                newSocket.emitter.emit('close', newSocket);
            });
            socket.on('end', () => {
                newSocket._isEof = true;
            });
            return newSocket;
        }
        throw new Error('No pending connections to accept');
    }
    close() {
        if (this.tcpSocket) {
            this.tcpSocket.end();
            this.tcpSocket = undefined;
        }
        if (this.udpSocket) {
            this.udpSocket.close();
            this.udpSocket = undefined;
        }
        if (this._server) {
            this._server.close();
            this._server = undefined;
        }
        this._isEof = true;
    }
    flush(_dir) {
        // Sockets auto-flush
    }
    async read(buffer, offset = 0, count = -1) {
        // Wait for data to arrive if buffer is empty
        if (this._dataBuffer.length === 0 && !this._isEof && this.tcpSocket) {
            const maxWaitMs = 5000; // 5 second timeout
            const pollIntervalMs = 10; // Check every 10ms
            const startTime = Date.now();
            // Wait for data to arrive via async events
            while (this._dataBuffer.length === 0 && !this._isEof) {
                await Bun.sleep(pollIntervalMs);
                if (Date.now() - startTime > maxWaitMs) {
                    throw new Error('Socket read timeout');
                }
            }
        }
        // Return null if still no data (EOF or no data arrived)
        if (this._dataBuffer.length === 0) {
            return null;
        }
        // Calculate total available data
        const totalAvailable = this._dataBuffer.reduce((sum, buf) => sum + buf.length, 0);
        const toRead = count === -1 ? totalAvailable : Math.min(count, totalAvailable);
        if (toRead === 0) {
            return null;
        }
        // For ByteArray, handle offset:
        // - offset = -1 or 0 with ByteArray: append at current writePosition
        // - offset > 0: write at specific offset
        let actualOffset = offset;
        if ('writePosition' in buffer && typeof buffer.writePosition === 'number') {
            if (offset === -1 || offset === 0) {
                // Append at current write position
                actualOffset = buffer.writePosition;
            }
        }
        else if (offset === -1) {
            // For regular Uint8Array, -1 means start at 0
            actualOffset = 0;
        }
        let bytesRead = 0;
        let currentOffset = actualOffset;
        while (bytesRead < toRead && this._dataBuffer.length > 0) {
            const chunk = this._dataBuffer[0];
            const bytesToCopy = Math.min(chunk.length, toRead - bytesRead);
            for (let i = 0; i < bytesToCopy; i++) {
                buffer[currentOffset++] = chunk[i];
            }
            bytesRead += bytesToCopy;
            if (bytesToCopy === chunk.length) {
                // Consumed entire chunk
                this._dataBuffer.shift();
            }
            else {
                // Partial consumption - keep remaining data
                this._dataBuffer[0] = Buffer.from(chunk.subarray(bytesToCopy));
            }
        }
        // If buffer is a ByteArray, update its write position
        if ('writePosition' in buffer && typeof buffer.writePosition === 'number') {
            buffer.writePosition = actualOffset + bytesRead;
        }
        return bytesRead;
    }
    async write(...data) {
        let totalWritten = 0;
        for (const item of data) {
            let buffer;
            if (typeof item === 'string') {
                buffer = Buffer.from(item);
            }
            else if (item instanceof Uint8Array) {
                buffer = Buffer.from(item);
            }
            else {
                buffer = Buffer.from(JSON.stringify(item));
            }
            if (this.tcpSocket) {
                // Wrap in promise to make write async
                await new Promise((resolve, reject) => {
                    const success = this.tcpSocket.write(buffer, (err) => {
                        if (err)
                            reject(err);
                        else
                            resolve();
                    });
                    // If write returns true (not buffered), resolve immediately
                    if (success)
                        resolve();
                });
                totalWritten += buffer.length;
            }
            else if (this.udpSocket) {
                // UDP send would need destination address
                totalWritten += buffer.length;
            }
        }
        return totalWritten;
    }
    on(name, observer) {
        this.emitter.on(name, observer);
        return this;
    }
    off(name, observer) {
        this.emitter.off(name, observer);
    }
    /**
     * Get local address
     */
    get address() {
        if (this.tcpSocket) {
            const addr = this.tcpSocket.address();
            if (typeof addr === 'object' && 'address' in addr && 'port' in addr) {
                return addr;
            }
        }
        return null;
    }
    /**
     * Get remote address
     */
    get remoteAddress() {
        if (this.tcpSocket) {
            return {
                address: this.tcpSocket.remoteAddress || '',
                port: this.tcpSocket.remotePort || 0
            };
        }
        return null;
    }
}
//# sourceMappingURL=Socket.js.map