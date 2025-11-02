/**
 * Http - HTTP client
 *
 * Provides HTTP/HTTPS client functionality
 * @spec ejs
 * @stability evolving
 */
import { Stream } from './streams/Stream';
import { Uri } from './utilities/Uri';
import { Path } from './Path';
import { Emitter } from './async/Emitter';
import { ByteArray } from './streams/ByteArray';
import { createHash } from 'crypto';
export class Http extends Stream {
    // HTTP status code constants
    static Continue = 100;
    static Ok = 200;
    static Created = 201;
    static Accepted = 202;
    static NotAuthoritative = 203;
    static NoContent = 204;
    static Reset = 205;
    static PartialContent = 206;
    static MultipleChoice = 300;
    static MovedPermanently = 301;
    static MovedTemporarily = 302;
    static SeeOther = 303;
    static NotModified = 304;
    static UseProxy = 305;
    static BadRequest = 400;
    static Unauthorized = 401;
    static PaymentRequired = 402;
    static Forbidden = 403;
    static NotFound = 404;
    static BadMethod = 405;
    static NotAcceptable = 406;
    static ProxyAuthRequired = 407;
    static RequestTimeout = 408;
    static Conflict = 409;
    static Gone = 410;
    static LengthRequired = 411;
    static PrecondFailed = 412;
    static EntityTooLarge = 413;
    static UriTooLong = 414;
    static UnsupportedMedia = 415;
    static BadRange = 416;
    static ServerError = 500;
    static NotImplemented = 501;
    static BadGateway = 502;
    static ServiceUnavailable = 503;
    static GatewayTimeout = 504;
    static VersionNotSupported = 505;
    _uri = null;
    _method = 'GET';
    _headers = {};
    _response = '';
    _status = null;
    _statusMessage = '';
    _responseHeaders = {};
    _finalized = false;
    _followRedirects = false;
    _retries = 2;
    _cache = true; // Enable HTTP caching by default
    emitter = new Emitter();
    credentials;
    _digestAuth;
    _ca;
    _certificate;
    _key;
    _verify = true;
    _verifyIssuer = true;
    _bodyLength = -1;
    _chunked = false;
    _encoding = 'utf-8';
    _limits = {
        chunk: 8192,
        connReuse: 5,
        headers: 4096,
        header: 32768,
        inactivityTimeout: 30,
        receive: 4194304,
        requestTimeout: 60,
        stageBuffer: 4096,
        transmission: 4194304
    };
    _readPosition = 0;
    _pendingRequest;
    _requestStream;
    _streamController;
    /**
     * Create an Http client
     * @param uri Optional initial URI
     */
    constructor(uri) {
        super();
        if (uri) {
            this._uri = typeof uri === 'string' ? new Uri(uri) : uri;
        }
        // Set default User-Agent header
        this._headers['User-Agent'] = 'Embedthis-http';
    }
    /**
     * Fetch a URL (convenience method)
     * @param uri URI to fetch
     * @param method HTTP method
     * @param ...data Data to send
     * @returns Response string
     */
    static async fetch(uri, method = 'GET', ...data) {
        const http = new Http();
        http.connect(method, uri, ...data);
        await http.finalize();
        return http.response;
    }
    /**
     * Async mode (deprecated - all operations are async)
     * @deprecated All HTTP operations are now async by default
     */
    get async() {
        return true;
    }
    set async(_enable) {
        // No-op: all operations are async
    }
    /**
     * CA certificate bundle file
     */
    get ca() {
        return this._ca || null;
    }
    set ca(bundle) {
        this._ca = bundle || undefined;
    }
    /**
     * Client certificate file
     */
    get certificate() {
        return this._certificate || null;
    }
    set certificate(certFile) {
        this._certificate = certFile || undefined;
    }
    /**
     * Private key file
     */
    get key() {
        return this._key || null;
    }
    set key(keyFile) {
        this._key = keyFile || undefined;
    }
    close() {
        // HTTP connections are managed by fetch API
    }
    /**
     * Response content length
     */
    get contentLength() {
        const length = this._responseHeaders['content-length'];
        return length ? parseInt(length) : -1;
    }
    /**
     * Response content type
     */
    get contentType() {
        return this._responseHeaders['content-type'] || '';
    }
    set contentType(value) {
        this._headers['Content-Type'] = value;
    }
    /**
     * Response date
     */
    get date() {
        const dateStr = this._responseHeaders['date'];
        return dateStr ? new Date(dateStr) : null;
    }
    /**
     * Request body length (for setting Content-Length)
     */
    get bodyLength() {
        return this._bodyLength;
    }
    set bodyLength(length) {
        this._bodyLength = length;
        if (length >= 0) {
            this.setHeader('Content-Length', length);
        }
    }
    /**
     * Use chunked transfer encoding
     */
    get chunked() {
        return this._chunked;
    }
    set chunked(enable) {
        this._chunked = enable;
        if (enable) {
            this.setHeader('Transfer-Encoding', 'chunked');
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
     * Response content encoding
     */
    get contentEncoding() {
        return this._responseHeaders['content-encoding'] || '';
    }
    /**
     * Expiration date from response
     */
    get expires() {
        const expiresStr = this._responseHeaders['expires'];
        return expiresStr ? new Date(expiresStr) : null;
    }
    /**
     * MIME type from content-type header
     */
    get mimeType() {
        const contentType = this.contentType;
        return contentType.split(';')[0].trim();
    }
    /**
     * Status code (alias for status)
     * If request is still pending, waits for completion first
     */
    get code() {
        if (this._pendingRequest) {
            throw new Error('Status not ready - use await http.finalize() or await http.wait() first');
        }
        return this._status;
    }
    /**
     * Status code as string
     */
    get codeString() {
        return this._status ? String(this._status) : '';
    }
    /**
     * Check if response data is available
     */
    get available() {
        return Math.max(0, this._response.length - this._readPosition);
    }
    /**
     * Get resource limits
     */
    get limits() {
        return { ...this._limits };
    }
    /**
     * Set resource limits
     * @param limits Limits object with properties like chunk, connReuse, etc.
     */
    setLimits(limits) {
        this._limits = { ...this._limits, ...limits };
    }
    /**
     * Get connection information
     */
    get info() {
        return {
            uri: this._uri?.toString(),
            method: this._method,
            status: this._status,
            statusMessage: this._statusMessage,
            contentLength: this.contentLength,
            contentType: this.contentType,
            isSecure: this.isSecure
        };
    }
    flush(_dir) {
        // Not applicable for HTTP
    }
    /**
     * Follow redirects automatically
     */
    get followRedirects() {
        return this._followRedirects;
    }
    set followRedirects(flag) {
        this._followRedirects = flag;
    }
    /**
     * Enable or disable HTTP caching
     * When enabled, Bun's fetch() may cache responses according to HTTP cache headers
     * When disabled, sets cache: 'no-store' to prevent any caching
     */
    get cache() {
        return this._cache;
    }
    set cache(flag) {
        this._cache = flag;
    }
    /**
     * Finalize the request and close any open stream
     * Call this after writing data with write() to close the stream and send the request
     * @returns Promise that resolves to the HTTP status code
     */
    async finalize() {
        if (this._streamController) {
            this._streamController.close();
        }
        this._finalized = true;
        // If there's a pending request, wait for it to complete
        if (this._pendingRequest) {
            await this._pendingRequest;
            this._pendingRequest = undefined;
        }
        else if (this._method && this._uri) {
            // No pending request - start one now (e.g., GET with no data)
            this._pendingRequest = this._performRequest([]);
            await this._pendingRequest;
            this._pendingRequest = undefined;
        }
        return this._status || 0;
    }
    /**
     * Check if request is finalized
     */
    get finalized() {
        return this._finalized;
    }
    /**
     * Connect and make an HTTP request
     * For streaming requests, this sets up the request but doesn't send until finalize() is called
     * For non-streaming requests with data, the request is sent immediately
     * @param method HTTP method
     * @param uri URI to request
     * @param ...data Data to send (if provided, sends immediately)
     * @returns This Http object for chaining (NOT a Promise - use finalize()/wait() to await completion)
     */
    connect(method, uri, ...data) {
        this._method = method;
        if (uri) {
            const uriString = typeof uri === 'string' ? uri : uri.toString();
            // Validate URI for HTML injection attempts (security check)
            if (/<[^>]+>/.test(uriString)) {
                throw new Error('Invalid URI: HTML tags not allowed in URL');
            }
            this._uri = typeof uri === 'string' ? new Uri(uri) : uri;
        }
        if (!this._uri) {
            throw new Error('No URI specified for HTTP request');
        }
        // If data is provided, send the request immediately (non-streaming)
        // Otherwise, defer until finalize() is called (streaming mode)
        if (data.length > 0) {
            this._pendingRequest = this._performRequest(data);
        }
        return this;
    }
    /**
     * Complete partial URLs for fetch API
     * Handles:
     * - '4100/index.html' -> 'http://127.0.0.1:4100/index.html'
     * - '127.0.0.1/index.html' -> 'http://127.0.0.1/index.html'
     * - ':4100/index.html' -> 'http://127.0.0.1:4100/index.html'
     * @param url URL string to complete
     * @returns Complete URL with scheme
     */
    _completeUrl(url) {
        // Already has a scheme (http:// or https://)
        if (/^https?:\/\//i.test(url)) {
            return url;
        }
        // Port number at start: '4100/path' -> 'http://127.0.0.1:4100/path'
        if (/^\d+\//.test(url)) {
            return `http://127.0.0.1:${url}`;
        }
        // Colon-port at start: ':4100/path' -> 'http://127.0.0.1:4100/path'
        if (/^:\d+/.test(url)) {
            return `http://127.0.0.1${url}`;
        }
        // IP address or hostname without scheme: '127.0.0.1/path' -> 'http://127.0.0.1/path'
        if (/^[\w\.\-]+[\/:]/.test(url)) {
            return `http://${url}`;
        }
        // Default case: assume localhost with path
        return `http://127.0.0.1/${url}`;
    }
    /**
     * Parse WWW-Authenticate Digest challenge header
     * @param header WWW-Authenticate header value
     * @returns Parsed challenge or null if invalid
     */
    _parseDigestChallenge(header) {
        if (!header.startsWith('Digest ')) {
            return null;
        }
        const challenge = header.substring(7);
        const params = {};
        // Parse key="value" or key=value pairs
        const regex = /(\w+)=(?:"([^"]+)"|([^\s,]+))/g;
        let match;
        while ((match = regex.exec(challenge)) !== null) {
            const key = match[1];
            const value = match[2] || match[3];
            params[key] = value;
        }
        // Validate required fields
        if (!params.realm || !params.nonce) {
            return null;
        }
        return {
            realm: params.realm,
            nonce: params.nonce,
            algorithm: params.algorithm || 'MD5',
            qop: params.qop,
            opaque: params.opaque
        };
    }
    /**
     * Compute hash using specified algorithm
     * @param algorithm Hash algorithm (MD5, SHA-256, SHA-512-256)
     * @param data Data to hash
     * @returns Hex-encoded hash
     */
    _digestHash(algorithm, data) {
        const normalizedAlgo = algorithm.toUpperCase().replace('-', '');
        let hashAlgo;
        switch (normalizedAlgo) {
            case 'MD5':
                hashAlgo = 'md5';
                break;
            case 'SHA256':
                hashAlgo = 'sha256';
                break;
            case 'SHA512256':
            case 'SHA512-256':
                hashAlgo = 'sha512-256';
                break;
            default:
                hashAlgo = 'md5';
        }
        const hash = createHash(hashAlgo);
        hash.update(data);
        return hash.digest('hex');
    }
    /**
     * Generate cryptographically secure client nonce
     * @returns Base64-encoded random string
     */
    _generateCnonce() {
        const randomBytes = new Uint8Array(16);
        crypto.getRandomValues(randomBytes);
        return btoa(String.fromCharCode(...randomBytes));
    }
    /**
     * Compute digest response
     * @param method HTTP method
     * @param uri Request URI
     * @param body Request body (for qop=auth-int)
     * @returns Computed digest response
     */
    _computeDigestResponse(method, uri, body) {
        if (!this._digestAuth) {
            throw new Error('Digest auth not initialized');
        }
        const { username, password, realm, nonce, algorithm, qop, nc } = this._digestAuth;
        // Compute HA1 = H(username:realm:password)
        const ha1 = this._digestHash(algorithm, `${username}:${realm}:${password}`);
        // Compute HA2
        let ha2;
        if (qop === 'auth-int' && body) {
            // HA2 = H(method:uri:H(body))
            const bodyHash = this._digestHash(algorithm, body);
            ha2 = this._digestHash(algorithm, `${method}:${uri}:${bodyHash}`);
        }
        else {
            // HA2 = H(method:uri)
            ha2 = this._digestHash(algorithm, `${method}:${uri}`);
        }
        // Compute response
        let response;
        if (qop) {
            // Generate cnonce
            const cnonce = this._generateCnonce();
            const ncHex = nc.toString(16).padStart(8, '0');
            // response = H(HA1:nonce:nc:cnonce:qop:HA2)
            response = this._digestHash(algorithm, `${ha1}:${nonce}:${ncHex}:${cnonce}:${qop}:${ha2}`);
            this._digestAuth.cnonce = cnonce;
        }
        else {
            // response = H(HA1:nonce:HA2)
            response = this._digestHash(algorithm, `${ha1}:${nonce}:${ha2}`);
        }
        return response;
    }
    /**
     * Build Authorization header for digest authentication
     * @param method HTTP method
     * @param uri Request URI
     * @param body Request body
     * @returns Authorization header value
     */
    _buildDigestAuthHeader(method, uri, body) {
        if (!this._digestAuth) {
            throw new Error('Digest auth not initialized');
        }
        const response = this._computeDigestResponse(method, uri, body);
        const { username, realm, nonce, algorithm, qop, opaque, nc } = this._digestAuth;
        const cnonce = this._digestAuth.cnonce;
        let header = `Digest username="${username}"`;
        header += `, realm="${realm}"`;
        header += `, nonce="${nonce}"`;
        header += `, uri="${uri}"`;
        header += `, response="${response}"`;
        if (algorithm && algorithm !== 'MD5') {
            header += `, algorithm=${algorithm}`;
        }
        if (qop) {
            header += `, qop=${qop}`;
            header += `, nc=${nc.toString(16).padStart(8, '0')}`;
            header += `, cnonce="${cnonce}"`;
        }
        if (opaque) {
            header += `, opaque="${opaque}"`;
        }
        return header;
    }
    /**
     * Check if digest auth can be reused for this URI
     * @param uri Request URI
     * @returns True if nonce can be reused
     */
    _canReuseDigestAuth(uri) {
        if (!this._digestAuth) {
            return false;
        }
        // Reuse nonce if it's the same URI or no URI was stored yet
        return !this._digestAuth.lastUri || this._digestAuth.lastUri === uri;
    }
    /**
     * Perform HTTP request
     */
    async _performRequest(data) {
        if (!this._uri)
            return;
        // Set default User-Agent if not already set
        if (!this._headers['User-Agent'] && !this._headers['user-agent']) {
            this._headers['User-Agent'] = 'Embedthis-http';
        }
        const fetchOptions = {
            method: this._method,
            headers: this._headers,
            redirect: this._followRedirects ? 'follow' : 'manual',
            cache: this._cache ? 'default' : 'no-store' // Control HTTP caching
        };
        // Set timeout if configured
        const timeoutMs = this._limits.requestTimeout * 1000;
        if (timeoutMs > 0) {
            const controller = new AbortController();
            const timeoutId = setTimeout(() => controller.abort(), timeoutMs);
            fetchOptions.signal = controller.signal;
            try {
                await this._performFetchRequest(fetchOptions, data);
                clearTimeout(timeoutId);
            }
            catch (error) {
                clearTimeout(timeoutId);
                if (error.name === 'AbortError') {
                    throw new Error(`Request timeout after ${this._limits.requestTimeout}s`);
                }
                throw error;
            }
        }
        else {
            await this._performFetchRequest(fetchOptions, data);
        }
    }
    async _performFetchRequest(fetchOptions, data) {
        if (!this._uri)
            return;
        // Handle request body - prioritize accumulated stream from write() calls
        if (this._requestStream) {
            fetchOptions.body = this._requestStream;
            fetchOptions.duplex = 'half'; // Required for streaming request bodies
        }
        else if (data.length > 0 && ['POST', 'PUT', 'PATCH'].includes(this._method)) {
            const formattedData = this._formatData(data);
            fetchOptions.body = formattedData;
            // Add duplex mode if body is a ReadableStream
            if (formattedData instanceof ReadableStream) {
                ;
                fetchOptions.duplex = 'half';
            }
        }
        // Complete partial URLs before passing to fetch
        const url = this._completeUrl(this._uri.toString());
        const uri = this._uri.toString();
        // Handle authentication
        if (this.credentials) {
            if (this.credentials.type === 'digest' || (this.credentials.type === undefined && this._digestAuth)) {
                // Check if we can reuse digest auth (preemptive authentication)
                if (this._canReuseDigestAuth(uri)) {
                    this._digestAuth.nc++;
                    this._digestAuth.lastUri = uri;
                    const authHeader = this._buildDigestAuthHeader(this._method, uri, typeof fetchOptions.body === 'string' ? fetchOptions.body : undefined);
                    fetchOptions.headers = {
                        ...fetchOptions.headers,
                        'Authorization': authHeader
                    };
                }
            }
            else if (this.credentials.type === 'basic') {
                // Explicit basic authentication
                const auth = btoa(`${this.credentials.username}:${this.credentials.password}`);
                this._headers['Authorization'] = `Basic ${auth}`;
            }
            // If type is undefined and no digest state, don't send auth preemptively
            // Wait for 401 to determine auth type
        }
        try {
            let response = await fetch(url, fetchOptions);
            // Handle 401 Unauthorized - auto-detect auth type from server response
            if (response.status === 401 && this.credentials) {
                const wwwAuth = response.headers.get('WWW-Authenticate');
                // Try Digest authentication
                if (wwwAuth?.startsWith('Digest ')) {
                    const challenge = this._parseDigestChallenge(wwwAuth);
                    if (challenge) {
                        // Initialize or update digest auth state
                        this._digestAuth = {
                            username: this.credentials.username,
                            password: this.credentials.password,
                            realm: challenge.realm,
                            nonce: challenge.nonce,
                            algorithm: challenge.algorithm,
                            qop: challenge.qop,
                            opaque: challenge.opaque,
                            nc: 1,
                            lastUri: uri
                        };
                        // Build digest authorization header
                        const authHeader = this._buildDigestAuthHeader(this._method, uri, typeof fetchOptions.body === 'string' ? fetchOptions.body : undefined);
                        // Retry request with digest auth
                        fetchOptions.headers = {
                            ...fetchOptions.headers,
                            'Authorization': authHeader
                        };
                        response = await fetch(url, fetchOptions);
                    }
                }
                // Try Basic authentication
                else if (wwwAuth?.startsWith('Basic ')) {
                    const auth = btoa(`${this.credentials.username}:${this.credentials.password}`);
                    fetchOptions.headers = {
                        ...fetchOptions.headers,
                        'Authorization': `Basic ${auth}`
                    };
                    response = await fetch(url, fetchOptions);
                }
            }
            this._status = response.status;
            this._statusMessage = response.statusText;
            response.headers.forEach((value, key) => {
                this._responseHeaders[key.toLowerCase()] = value;
            });
            // If transfer-encoding is chunked, remove any content-length header
            // as it's invalid per HTTP spec and fetch() may add incorrect values
            if (this._responseHeaders['transfer-encoding']?.toLowerCase().includes('chunked')) {
                delete this._responseHeaders['content-length'];
            }
            this._response = await response.text();
            this._readPosition = 0;
            this.emitter.emit('complete', this);
        }
        catch (error) {
            this.emitter.emit('error', error);
            throw error;
        }
    }
    /**
     * Format data for request body
     * Supports: string, Uint8Array, ReadableStream, or objects (JSON serialized)
     */
    _formatData(data) {
        if (data.length === 1) {
            const item = data[0];
            if (typeof item === 'string') {
                return item;
            }
            else if (item instanceof Uint8Array) {
                return item;
            }
            else if (item instanceof ReadableStream) {
                return item;
            }
            else {
                return JSON.stringify(item);
            }
        }
        return data.map(d => String(d)).join('');
    }
    /**
     * POST request with form data
     * @param uri URI
     * @param data Form data object
     * @returns This Http object for chaining
     */
    form(uri, data) {
        this.setHeader('Content-Type', 'application/x-www-form-urlencoded');
        const encoded = Uri.encodeQuery(data);
        return this.connect('POST', uri, encoded);
    }
    /**
     * POST request with JSON data
     * @param uri URI
     * @param ...data Data objects
     * @returns This Http object for chaining
     */
    jsonForm(uri, ...data) {
        this.setHeader('Content-Type', 'application/json');
        return this.connect('POST', uri, JSON.stringify(data.length === 1 ? data[0] : data));
    }
    /**
     * GET request
     * @param uri URI
     * @param ...data Optional data
     * @returns This Http object for chaining
     */
    get(uri, ...data) {
        return this.connect('GET', uri, ...data);
    }
    /**
     * HEAD request
     * @param uri URI
     * @returns This Http object for chaining
     */
    head(uri) {
        return this.connect('HEAD', uri);
    }
    /**
     * POST request
     * @param uri URI
     * @param ...data Data to post
     * @returns This Http object for chaining
     */
    post(uri, ...data) {
        return this.connect('POST', uri, ...data);
    }
    /**
     * PUT request
     * @param uri URI
     * @param ...data Data to put
     * @returns This Http object for chaining
     */
    put(uri, ...data) {
        return this.connect('PUT', uri, ...data);
    }
    /**
     * DELETE request
     * @param uri URI
     * @returns This Http object for chaining
     */
    del(uri) {
        return this.connect('DELETE', uri);
    }
    /**
     * Get request headers that will be sent
     */
    getRequestHeaders() {
        return { ...this._headers };
    }
    /**
     * Get a single response header
     * @param key Header key
     * @returns Header value or null
     */
    header(key) {
        return this._responseHeaders[key.toLowerCase()] || null;
    }
    /**
     * Get all response headers
     */
    get headers() {
        return { ...this._responseHeaders };
    }
    /**
     * Check if connection is secure (HTTPS)
     */
    get isSecure() {
        return this._uri?.scheme === 'https';
    }
    /**
     * Last modified date
     */
    get lastModified() {
        const lastMod = this._responseHeaders['last-modified'];
        return lastMod ? new Date(lastMod) : null;
    }
    /**
     * HTTP method
     */
    get method() {
        return this._method;
    }
    set method(name) {
        this._method = name;
    }
    read(buffer, offset = 0, count = -1) {
        if (this._pendingRequest) {
            throw new Error('Response not ready - use await http.finalize() or await http.wait() first');
        }
        if (this._readPosition >= this._response.length) {
            return null;
        }
        const remaining = this._response.length - this._readPosition;
        // If buffer is a ByteArray, reset writePosition first for reusable buffer pattern
        if (buffer instanceof ByteArray) {
            buffer.writePosition = offset;
        }
        // If buffer is a ByteArray and count is -1, limit to buffer's available room
        // This prevents overflow errors when ByteArray can't grow
        let toRead;
        if (buffer instanceof ByteArray && count === -1) {
            // Read up to the buffer's available room, or remaining data, whichever is smaller
            const bufferRoom = buffer._size - buffer.writePosition;
            toRead = Math.min(remaining, bufferRoom);
        }
        else {
            toRead = count === -1 ? remaining : Math.min(count, remaining);
        }
        if (toRead === 0) {
            return null;
        }
        // Convert response string to bytes
        const chunk = this._response.substring(this._readPosition, this._readPosition + toRead);
        const encoder = new TextEncoder();
        const bytes = encoder.encode(chunk);
        // If buffer is a ByteArray, use its write method
        if (buffer instanceof ByteArray) {
            // Write the bytes (writePosition already set above)
            buffer.write(bytes);
            this._readPosition += toRead;
            return bytes.length;
        }
        else {
            // For regular Uint8Array, just copy bytes
            for (let i = 0; i < bytes.length; i++) {
                buffer[offset + i] = bytes[i];
            }
            this._readPosition += toRead;
            return bytes.length;
        }
    }
    /**
     * Read response as string
     * If request is still pending, throws error (use finalize/wait first)
     * @param count Number of characters to read
     * @returns Response string
     */
    readString(count = -1) {
        if (this._pendingRequest) {
            throw new Error('Response not ready - use await http.finalize() or await http.wait() first');
        }
        if (count === -1) {
            const result = this._response.substring(this._readPosition);
            this._readPosition = this._response.length;
            return result;
        }
        const result = this._response.substring(this._readPosition, this._readPosition + count);
        this._readPosition += result.length;
        return result;
    }
    /**
     * Read response as lines
     * If request is still pending, throws error (use finalize/wait first)
     * @param count Number of lines
     * @returns Array of lines
     */
    readLines(count = -1) {
        if (this._pendingRequest) {
            throw new Error('Response not ready - use await http.finalize() or await http.wait() first');
        }
        const lines = this._response.split(/\r?\n/);
        // Remove trailing empty line if response ends with newline
        if (lines.length > 0 && lines[lines.length - 1] === '') {
            lines.pop();
        }
        return count === -1 ? lines : lines.slice(0, count);
    }
    /**
     * Read response as XML
     * @returns XML object
     */
    readXml() {
        // Would need XML parser
        return this._response;
    }
    /**
     * Reset the HTTP object
     */
    reset() {
        this._headers = {};
        this._response = '';
        this._status = null;
        this._responseHeaders = {};
        this._finalized = false;
        this._readPosition = 0;
        this._digestAuth = undefined;
        this._requestStream = undefined;
        this._streamController = undefined;
    }
    /**
     * Get response body
     * If request is still pending, waits for completion first
     */
    get response() {
        if (this._pendingRequest) {
            throw new Error('Response not ready - use await http.finalize() or await http.wait() first');
        }
        return this._response;
    }
    set response(data) {
        this._response = data;
    }
    /**
     * Get/set retry count
     */
    get retries() {
        return this._retries;
    }
    set retries(count) {
        this._retries = count;
    }
    /**
     * Get session cookie
     */
    get sessionCookie() {
        const cookie = this.header('Set-Cookie');
        if (cookie) {
            const match = cookie.match(/(-ejs-session-=[^;]*);/);
            return match ? match[1] : null;
        }
        return null;
    }
    /**
     * Set cookie header
     * @param cookie Cookie value
     */
    setCookie(cookie) {
        this.setHeader('Cookie', cookie);
    }
    /**
     * Set authentication credentials
     * @param username Username (null to clear credentials)
     * @param password Password (null to clear credentials)
     * @param type Authentication type: 'basic', 'digest', or undefined for auto-detection
     *
     * If type is not specified, the Http client automatically detects the auth type
     * from the server's WWW-Authenticate header when a 401 response is received.
     * This provides maximum flexibility - the same code works with both Basic and
     * Digest authentication servers.
     *
     * When Digest auth is used (either explicitly or auto-detected), the client
     * handles RFC 2617/7616 digest authentication challenge-response workflow,
     * supporting MD5, SHA-256, and SHA-512-256 algorithms.
     *
     * @example
     * // Auto-detect auth type (recommended - works with any server)
     * http.setCredentials('user', 'pass')
     * await http.get('/protected')  // Server determines auth type via 401 response
     *
     * @example
     * // Explicit basic authentication
     * http.setCredentials('user', 'pass', 'basic')
     *
     * @example
     * // Explicit digest authentication
     * http.setCredentials('user', 'pass', 'digest')
     */
    setCredentials(username, password, type = null) {
        if (username === null || password === null) {
            this.credentials = undefined;
            this._digestAuth = undefined;
            // Clear the Authorization header when credentials are removed
            delete this._headers['Authorization'];
        }
        else {
            this.credentials = { username, password, type: type || undefined };
            this._digestAuth = undefined; // Clear old digest state
        }
    }
    /**
     * Set a request header
     * @param key Header key
     * @param value Header value
     * @param overwrite Overwrite existing header
     */
    setHeader(key, value, overwrite = true) {
        const stringValue = typeof value === 'number' ? String(value) : value;
        if (overwrite || !(key in this._headers)) {
            this._headers[key] = stringValue;
        }
        else {
            this._headers[key] += ', ' + stringValue;
        }
    }
    /**
     * Add a request header (Ejscript compatibility alias for setHeader)
     * @param key Header key
     * @param value Header value
     * @param overwrite Overwrite existing header (default true)
     */
    addHeader(key, value, overwrite = true) {
        this.setHeader(key, value, overwrite);
    }
    /**
     * Set multiple request headers
     * @param headers Headers object
     * @param overwrite Overwrite existing headers
     */
    setHeaders(headers, overwrite = true) {
        for (const [key, value] of Object.entries(headers)) {
            this.setHeader(key, value, overwrite);
        }
    }
    /**
     * Remove a request header
     * @param key Header key to remove
     */
    removeHeader(key) {
        delete this._headers[key];
    }
    /**
     * Get HTTP status code
     * If request is still pending, waits for completion first
     */
    get status() {
        if (this._pendingRequest) {
            throw new Error('Status not ready - use await http.finalize() or await http.wait() first');
        }
        return this._status;
    }
    /**
     * Get HTTP status message
     */
    get statusMessage() {
        return this._statusMessage;
    }
    /**
     * Check if request was successful (2xx status)
     */
    get success() {
        return this._status !== null && this._status >= 200 && this._status < 300;
    }
    /**
     * Configure request tracing
     * @param options Trace options
     */
    trace(_options) {
        // Would configure debug tracing
    }
    /**
     * Upload files
     * @param uri URI
     * @param files Files to upload
     * @param fields Form fields
     * @returns This Http object for chaining
     */
    upload(uri, files, fields) {
        // Simplified multipart upload
        const boundary = '----FormBoundary' + Math.random().toString(36);
        this.setHeader('Content-Type', `multipart/form-data; boundary=${boundary}`);
        let body = '';
        // Add fields
        if (fields) {
            for (const [key, value] of Object.entries(fields)) {
                body += `--${boundary}\r\n`;
                body += `Content-Disposition: form-data; name="${key}"\r\n\r\n`;
                body += `${value}\r\n`;
            }
        }
        // Add files (simplified)
        if (typeof files === 'object') {
            for (const [key, file] of Object.entries(files)) {
                const filePath = new Path(String(file));
                body += `--${boundary}\r\n`;
                body += `Content-Disposition: form-data; name="${key}"; filename="${filePath.basename}"\r\n`;
                body += `Content-Type: ${filePath.mimeType}\r\n\r\n`;
                body += filePath.readString() || '';
                body += '\r\n';
            }
        }
        body += `--${boundary}--\r\n`;
        return this.connect('POST', uri, body);
    }
    /**
     * Get/set URI
     */
    get uri() {
        return this._uri;
    }
    set uri(newUri) {
        this._uri = typeof newUri === 'string' ? new Uri(newUri) : newUri;
    }
    /**
     * Verify peer certificates
     */
    get verify() {
        return this._verify;
    }
    set verify(enable) {
        this._verify = enable;
        this._verifyIssuer = enable;
    }
    /**
     * Verify certificate issuer
     */
    get verifyIssuer() {
        return this._verifyIssuer;
    }
    set verifyIssuer(enable) {
        this._verifyIssuer = enable;
    }
    /**
     * Wait for request to complete
     * @param timeout Timeout in milliseconds (-1 for infinite)
     * @returns Promise that resolves to true if completed, false if timeout
     */
    async wait(timeout = -1) {
        if (!this._pendingRequest) {
            // No pending request - start one now if method and URI are set
            if (this._method && this._uri) {
                this._pendingRequest = this._performRequest([]);
            }
            else {
                // No pending request and can't start one - return current status
                return this._status !== null;
            }
        }
        if (timeout === -1) {
            // Wait indefinitely
            await this._pendingRequest;
            this._pendingRequest = undefined;
            return this._status !== null;
        }
        // Wait with timeout
        try {
            await Promise.race([
                this._pendingRequest,
                new Promise((_, reject) => setTimeout(() => reject(new Error('timeout')), timeout))
            ]);
            this._pendingRequest = undefined;
            return this._status !== null;
        }
        catch (error) {
            if (error.message === 'timeout') {
                return false;
            }
            throw error;
        }
    }
    /**
     * Write data to request body for streaming uploads
     * @param data Data to write (string, Uint8Array, or any serializable object)
     * @returns Number of bytes written
     *
     * Use this method to build up a request body incrementally:
     * @example
     * http.connect('POST', uri)  // Set up POST request (non-blocking)
     * http.write('chunk1')
     * http.write('chunk2')
     * await http.finalize()  // Close stream and send request
     */
    write(...data) {
        if (!this._streamController) {
            // Create a new ReadableStream on first write
            this._requestStream = new ReadableStream({
                start: (controller) => {
                    this._streamController = controller;
                }
            });
            // Start the request now that we have a stream to send
            if (!this._pendingRequest) {
                this._pendingRequest = this._performRequest([]);
            }
        }
        let bytesWritten = 0;
        for (const item of data) {
            if (typeof item === 'string') {
                const encoded = new TextEncoder().encode(item);
                this._streamController.enqueue(encoded);
                bytesWritten += encoded.length;
            }
            else if (item instanceof Uint8Array) {
                this._streamController.enqueue(item);
                bytesWritten += item.length;
            }
            else {
                // Serialize objects as JSON
                const json = JSON.stringify(item);
                const encoded = new TextEncoder().encode(json);
                this._streamController.enqueue(encoded);
                bytesWritten += encoded.length;
            }
        }
        return bytesWritten;
    }
    on(name, observer) {
        this.emitter.on(name, observer);
        return this;
    }
    off(name, observer) {
        this.emitter.off(name, observer);
    }
}
//# sourceMappingURL=Http.js.map