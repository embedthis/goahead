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
export declare class Http extends Stream {
    static readonly Continue = 100;
    static readonly Ok = 200;
    static readonly Created = 201;
    static readonly Accepted = 202;
    static readonly NotAuthoritative = 203;
    static readonly NoContent = 204;
    static readonly Reset = 205;
    static readonly PartialContent = 206;
    static readonly MultipleChoice = 300;
    static readonly MovedPermanently = 301;
    static readonly MovedTemporarily = 302;
    static readonly SeeOther = 303;
    static readonly NotModified = 304;
    static readonly UseProxy = 305;
    static readonly BadRequest = 400;
    static readonly Unauthorized = 401;
    static readonly PaymentRequired = 402;
    static readonly Forbidden = 403;
    static readonly NotFound = 404;
    static readonly BadMethod = 405;
    static readonly NotAcceptable = 406;
    static readonly ProxyAuthRequired = 407;
    static readonly RequestTimeout = 408;
    static readonly Conflict = 409;
    static readonly Gone = 410;
    static readonly LengthRequired = 411;
    static readonly PrecondFailed = 412;
    static readonly EntityTooLarge = 413;
    static readonly UriTooLong = 414;
    static readonly UnsupportedMedia = 415;
    static readonly BadRange = 416;
    static readonly ServerError = 500;
    static readonly NotImplemented = 501;
    static readonly BadGateway = 502;
    static readonly ServiceUnavailable = 503;
    static readonly GatewayTimeout = 504;
    static readonly VersionNotSupported = 505;
    private _uri;
    private _method;
    private _headers;
    private _response;
    private _status;
    private _statusMessage;
    private _responseHeaders;
    private _finalized;
    private _followRedirects;
    private _retries;
    private _cache;
    private emitter;
    private credentials?;
    private _digestAuth?;
    private _ca?;
    private _certificate?;
    private _key?;
    private _verify;
    private _verifyIssuer;
    private _bodyLength;
    private _chunked;
    private _encoding;
    private _limits;
    private _readPosition;
    private _pendingRequest?;
    private _requestStream?;
    private _streamController?;
    /**
     * Create an Http client
     * @param uri Optional initial URI
     */
    constructor(uri?: Uri | string | null);
    /**
     * Fetch a URL (convenience method)
     * @param uri URI to fetch
     * @param method HTTP method
     * @param ...data Data to send
     * @returns Response string
     */
    static fetch(uri: Uri | string, method?: string, ...data: any[]): Promise<string>;
    /**
     * Async mode (deprecated - all operations are async)
     * @deprecated All HTTP operations are now async by default
     */
    get async(): boolean;
    set async(_enable: boolean);
    /**
     * CA certificate bundle file
     */
    get ca(): Path | null;
    set ca(bundle: Path | null);
    /**
     * Client certificate file
     */
    get certificate(): Path | null;
    set certificate(certFile: Path | null);
    /**
     * Private key file
     */
    get key(): Path | null;
    set key(keyFile: Path | null);
    close(): void;
    /**
     * Response content length
     */
    get contentLength(): number;
    /**
     * Response content type
     */
    get contentType(): string;
    set contentType(value: string);
    /**
     * Response date
     */
    get date(): Date | null;
    /**
     * Request body length (for setting Content-Length)
     */
    get bodyLength(): number;
    set bodyLength(length: number);
    /**
     * Use chunked transfer encoding
     */
    get chunked(): boolean;
    set chunked(enable: boolean);
    /**
     * Character encoding for serializing strings
     */
    get encoding(): string;
    set encoding(enc: string);
    /**
     * Response content encoding
     */
    get contentEncoding(): string;
    /**
     * Expiration date from response
     */
    get expires(): Date | null;
    /**
     * MIME type from content-type header
     */
    get mimeType(): string;
    /**
     * Status code (alias for status)
     * If request is still pending, waits for completion first
     */
    get code(): number | null;
    /**
     * Status code as string
     */
    get codeString(): string;
    /**
     * Check if response data is available
     */
    get available(): number;
    /**
     * Get resource limits
     */
    get limits(): Record<string, number>;
    /**
     * Set resource limits
     * @param limits Limits object with properties like chunk, connReuse, etc.
     */
    setLimits(limits: Record<string, number>): void;
    /**
     * Get connection information
     */
    get info(): Record<string, any>;
    flush(_dir?: number): void;
    /**
     * Follow redirects automatically
     */
    get followRedirects(): boolean;
    set followRedirects(flag: boolean);
    /**
     * Enable or disable HTTP caching
     * When enabled, Bun's fetch() may cache responses according to HTTP cache headers
     * When disabled, sets cache: 'no-store' to prevent any caching
     */
    get cache(): boolean;
    set cache(flag: boolean);
    /**
     * Finalize the request and close any open stream
     * Call this after writing data with write() to close the stream and send the request
     * @returns Promise that resolves to the HTTP status code
     */
    finalize(): Promise<number>;
    /**
     * Check if request is finalized
     */
    get finalized(): boolean;
    /**
     * Connect and make an HTTP request
     * For streaming requests, this sets up the request but doesn't send until finalize() is called
     * For non-streaming requests with data, the request is sent immediately
     * @param method HTTP method
     * @param uri URI to request
     * @param ...data Data to send (if provided, sends immediately)
     * @returns This Http object for chaining (NOT a Promise - use finalize()/wait() to await completion)
     */
    connect(method: string, uri?: Uri | string | null, ...data: any[]): Http;
    /**
     * Complete partial URLs for fetch API
     * Handles:
     * - '4100/index.html' -> 'http://127.0.0.1:4100/index.html'
     * - '127.0.0.1/index.html' -> 'http://127.0.0.1/index.html'
     * - ':4100/index.html' -> 'http://127.0.0.1:4100/index.html'
     * @param url URL string to complete
     * @returns Complete URL with scheme
     */
    private _completeUrl;
    /**
     * Parse WWW-Authenticate Digest challenge header
     * @param header WWW-Authenticate header value
     * @returns Parsed challenge or null if invalid
     */
    private _parseDigestChallenge;
    /**
     * Compute hash using specified algorithm
     * @param algorithm Hash algorithm (MD5, SHA-256, SHA-512-256)
     * @param data Data to hash
     * @returns Hex-encoded hash
     */
    private _digestHash;
    /**
     * Generate cryptographically secure client nonce
     * @returns Base64-encoded random string
     */
    private _generateCnonce;
    /**
     * Compute digest response
     * @param method HTTP method
     * @param uri Request URI
     * @param body Request body (for qop=auth-int)
     * @returns Computed digest response
     */
    private _computeDigestResponse;
    /**
     * Build Authorization header for digest authentication
     * @param method HTTP method
     * @param uri Request URI
     * @param body Request body
     * @returns Authorization header value
     */
    private _buildDigestAuthHeader;
    /**
     * Check if digest auth can be reused for this URI
     * @param uri Request URI
     * @returns True if nonce can be reused
     */
    private _canReuseDigestAuth;
    /**
     * Perform HTTP request
     */
    private _performRequest;
    private _performFetchRequest;
    /**
     * Format data for request body
     * Supports: string, Uint8Array, ReadableStream, or objects (JSON serialized)
     */
    private _formatData;
    /**
     * POST request with form data
     * @param uri URI
     * @param data Form data object
     * @returns This Http object for chaining
     */
    form(uri: Uri | string, data: Record<string, any>): Http;
    /**
     * POST request with JSON data
     * @param uri URI
     * @param ...data Data objects
     * @returns This Http object for chaining
     */
    jsonForm(uri: Uri | string, ...data: any[]): Http;
    /**
     * GET request
     * @param uri URI
     * @param ...data Optional data
     * @returns This Http object for chaining
     */
    get(uri?: Uri | string | null, ...data: any[]): Http;
    /**
     * HEAD request
     * @param uri URI
     * @returns This Http object for chaining
     */
    head(uri?: Uri | string | null): Http;
    /**
     * POST request
     * @param uri URI
     * @param ...data Data to post
     * @returns This Http object for chaining
     */
    post(uri?: Uri | string | null, ...data: any[]): Http;
    /**
     * PUT request
     * @param uri URI
     * @param ...data Data to put
     * @returns This Http object for chaining
     */
    put(uri?: Uri | string | null, ...data: any[]): Http;
    /**
     * DELETE request
     * @param uri URI
     * @returns This Http object for chaining
     */
    del(uri?: Uri | string | null): Http;
    /**
     * Get request headers that will be sent
     */
    getRequestHeaders(): Record<string, string>;
    /**
     * Get a single response header
     * @param key Header key
     * @returns Header value or null
     */
    header(key: string): string | null;
    /**
     * Get all response headers
     */
    get headers(): Record<string, string>;
    /**
     * Check if connection is secure (HTTPS)
     */
    get isSecure(): boolean;
    /**
     * Last modified date
     */
    get lastModified(): Date | null;
    /**
     * HTTP method
     */
    get method(): string;
    set method(name: string);
    read(buffer: Uint8Array, offset?: number, count?: number): number | null;
    /**
     * Read response as string
     * If request is still pending, throws error (use finalize/wait first)
     * @param count Number of characters to read
     * @returns Response string
     */
    readString(count?: number): string | null;
    /**
     * Read response as lines
     * If request is still pending, throws error (use finalize/wait first)
     * @param count Number of lines
     * @returns Array of lines
     */
    readLines(count?: number): string[] | null;
    /**
     * Read response as XML
     * @returns XML object
     */
    readXml(): any;
    /**
     * Reset the HTTP object
     */
    reset(): void;
    /**
     * Get response body
     * If request is still pending, waits for completion first
     */
    get response(): string;
    set response(data: string);
    /**
     * Get/set retry count
     */
    get retries(): number;
    set retries(count: number);
    /**
     * Get session cookie
     */
    get sessionCookie(): string | null;
    /**
     * Set cookie header
     * @param cookie Cookie value
     */
    setCookie(cookie: string): void;
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
    setCredentials(username: string | null, password: string | null, type?: string | null): void;
    /**
     * Set a request header
     * @param key Header key
     * @param value Header value
     * @param overwrite Overwrite existing header
     */
    setHeader(key: string, value: string | number, overwrite?: boolean): void;
    /**
     * Add a request header (Ejscript compatibility alias for setHeader)
     * @param key Header key
     * @param value Header value
     * @param overwrite Overwrite existing header (default true)
     */
    addHeader(key: string, value: string | number, overwrite?: boolean): void;
    /**
     * Set multiple request headers
     * @param headers Headers object
     * @param overwrite Overwrite existing headers
     */
    setHeaders(headers: Record<string, string | number>, overwrite?: boolean): void;
    /**
     * Remove a request header
     * @param key Header key to remove
     */
    removeHeader(key: string): void;
    /**
     * Get HTTP status code
     * If request is still pending, waits for completion first
     */
    get status(): number | null;
    /**
     * Get HTTP status message
     */
    get statusMessage(): string;
    /**
     * Check if request was successful (2xx status)
     */
    get success(): boolean;
    /**
     * Configure request tracing
     * @param options Trace options
     */
    trace(_options: any): void;
    /**
     * Upload files
     * @param uri URI
     * @param files Files to upload
     * @param fields Form fields
     * @returns This Http object for chaining
     */
    upload(uri: string | Uri, files: any, fields?: Record<string, any>): Http;
    /**
     * Get/set URI
     */
    get uri(): Uri | null;
    set uri(newUri: Uri | string | null);
    /**
     * Verify peer certificates
     */
    get verify(): boolean;
    set verify(enable: boolean);
    /**
     * Verify certificate issuer
     */
    get verifyIssuer(): boolean;
    set verifyIssuer(enable: boolean);
    /**
     * Wait for request to complete
     * @param timeout Timeout in milliseconds (-1 for infinite)
     * @returns Promise that resolves to true if completed, false if timeout
     */
    wait(timeout?: number): Promise<boolean>;
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
    write(...data: any[]): number;
    on(name: string, observer: Function): this;
    off(name: string, observer: Function): void;
}
//# sourceMappingURL=Http.d.ts.map