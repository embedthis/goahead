/**
 * Uri - URI parsing and manipulation
 *
 * Provides URI/URL parsing, encoding, and MIME type detection
 * @spec ejs
 * @stability evolving
 */
import { Path } from '../Path';
import * as path from 'path';
export class Uri {
    _url = null;
    _path;
    _scheme = null;
    _host = null;
    _port = null;
    _query = null;
    _reference = null;
    /**
     * Create a Uri from a string or object
     * @param uri URI string or object with components
     */
    constructor(uri) {
        if (typeof uri === 'object') {
            // Construct from object components
            const obj = uri;
            this._scheme = obj.scheme || null;
            this._host = obj.host || null;
            this._port = obj.port || null;
            this._path = obj.path || '';
            this._query = obj.query || null;
            this._reference = obj.reference || null;
            // Build URL if we have scheme and host
            if (this._scheme && this._host) {
                try {
                    let urlStr = `${this._scheme}://${this._host}`;
                    if (this._port)
                        urlStr += `:${this._port}`;
                    urlStr += this._path || '/';
                    if (this._query)
                        urlStr += `?${this._query}`;
                    if (this._reference)
                        urlStr += `#${this._reference}`;
                    this._url = new URL(urlStr);
                }
                catch {
                    // Invalid URL construction
                }
            }
        }
        else {
            // Check for partial URLs like "14100/index.html", ":14100/page", "host:port/path"
            const colonIndex = uri.indexOf(':');
            const slashIndex = uri.indexOf('/');
            // Check if it starts with just a port number (e.g., "14100/index.html" or ":14100/page")
            if (slashIndex > 0 && !uri.includes('://')) {
                const beforeSlash = uri.substring(0, slashIndex);
                let portStr = beforeSlash;
                let hostStr = '';
                // Handle ":port/path" format
                if (beforeSlash.startsWith(':')) {
                    portStr = beforeSlash.substring(1);
                    hostStr = '';
                }
                // Handle "port/path" format (just digits)
                else if (/^\d+$/.test(beforeSlash)) {
                    portStr = beforeSlash;
                    hostStr = '';
                }
                // Handle "host:port/path" format
                else if (colonIndex > 0 && colonIndex < slashIndex) {
                    hostStr = beforeSlash.substring(0, colonIndex);
                    portStr = beforeSlash.substring(colonIndex + 1);
                }
                else {
                    portStr = '';
                }
                const portNum = parseInt(portStr, 10);
                if (!isNaN(portNum) && portNum > 0 && portNum <= 65535) {
                    // This is a partial URL with port
                    this._host = hostStr || '127.0.0.1';
                    this._port = portNum;
                    this._path = uri.substring(slashIndex);
                    this._scheme = 'http';
                    this._rebuildUrl();
                    return;
                }
            }
            // Check if it's host:port format (no path)
            if (colonIndex > 0 && colonIndex < uri.length - 1 && !uri.includes('://')) {
                const possiblePort = uri.substring(colonIndex + 1);
                const portNum = parseInt(possiblePort, 10);
                // Check if it looks like a port number (valid number, only digits after colon, no slashes)
                if (!isNaN(portNum) && possiblePort === portNum.toString() &&
                    portNum > 0 && portNum <= 65535 && !possiblePort.includes('/')) {
                    // This is host:port format
                    this._host = uri.substring(0, colonIndex) || '127.0.0.1';
                    this._port = portNum;
                    this._path = '/';
                    this._scheme = 'http';
                    this._rebuildUrl();
                    return;
                }
            }
            try {
                // Try to parse as full URL
                this._url = new URL(uri);
                this._scheme = this._url.protocol.replace(':', '');
                this._host = this._url.hostname;
                this._port = this._url.port ? parseInt(this._url.port) : null;
                this._path = this._url.pathname;
                this._query = this._url.search ? this._url.search.substring(1) : null;
                this._reference = this._url.hash ? this._url.hash.substring(1) : null;
            }
            catch {
                // Not a full URL, treat as path
                this._path = uri;
            }
        }
    }
    /**
     * Get the complete URI as a string
     */
    toString() {
        return this._url ? this._url.toString() : this._path;
    }
    /**
     * Get the scheme/protocol (e.g., 'http', 'https')
     */
    get scheme() {
        return this._scheme;
    }
    /**
     * Set the scheme/protocol
     */
    set scheme(value) {
        this._scheme = value;
        this._rebuildUrl();
    }
    /**
     * Get the host
     */
    get host() {
        return this._host;
    }
    /**
     * Set the host
     */
    set host(value) {
        this._host = value;
        this._rebuildUrl();
    }
    /**
     * Get the port
     */
    get port() {
        return this._port;
    }
    /**
     * Set the port
     */
    set port(value) {
        this._port = value;
        this._rebuildUrl();
    }
    /**
     * Get the path portion
     */
    get path() {
        return this._path;
    }
    /**
     * Set the path
     */
    set path(value) {
        this._path = value;
        this._rebuildUrl();
    }
    /**
     * Get the query string (without '?')
     */
    get query() {
        return this._query;
    }
    /**
     * Set the query string
     */
    set query(value) {
        this._query = value;
        this._rebuildUrl();
    }
    /**
     * Get the fragment/hash (without '#')
     */
    get hash() {
        return this._reference;
    }
    /**
     * Get reference portion (same as hash)
     */
    get reference() {
        return this._reference;
    }
    /**
     * Set the reference/hash
     */
    set reference(value) {
        this._reference = value;
        this._rebuildUrl();
    }
    /**
     * Get the complete URI string
     */
    get uri() {
        return this.toString();
    }
    /**
     * Get the TCP/IP address (host:port)
     */
    get address() {
        const host = this._host || '127.0.0.1';
        return this._port ? `${host}:${this._port}` : host;
    }
    /**
     * Get all URI components as an object
     */
    get components() {
        return {
            scheme: this._scheme,
            host: this._host,
            port: this._port,
            path: this._path,
            query: this._query,
            reference: this._reference
        };
    }
    /**
     * Check if URI is absolute
     */
    get isAbsolute() {
        return this._scheme !== null && this._host !== null;
    }
    /**
     * Check if URI is relative
     */
    get isRelative() {
        return !this.isAbsolute;
    }
    /**
     * Check if URI has a scheme
     */
    get hasScheme() {
        return this._scheme !== null;
    }
    /**
     * Check if URI has a host
     */
    get hasHost() {
        return this._host !== null;
    }
    /**
     * Check if URI has a port
     */
    get hasPort() {
        return this._port !== null;
    }
    /**
     * Check if URI has a query string
     */
    get hasQuery() {
        return this._query !== null && this._query.length > 0;
    }
    /**
     * Check if URI has a reference/hash
     */
    get hasReference() {
        return this._reference !== null && this._reference.length > 0;
    }
    /**
     * Check if path has an extension
     */
    get hasExtension() {
        return this.extension.length > 0;
    }
    /**
     * Check if path represents a directory (ends with /)
     */
    get isDir() {
        return this._path.endsWith('/');
    }
    /**
     * Check if path represents a regular file (doesn't end with /)
     */
    get isRegular() {
        return !this.isDir;
    }
    /**
     * Get the basename of the path
     */
    get basename() {
        return new Uri(path.basename(this._path));
    }
    /**
     * Get the directory name of the path
     */
    get dirname() {
        return new Uri(path.dirname(this._path));
    }
    /**
     * Get the file extension (without dot)
     */
    get extension() {
        const ext = path.extname(this._path);
        return ext.startsWith('.') ? ext.substring(1) : ext;
    }
    /**
     * Get the filename (basename without extension)
     */
    get filename() {
        const base = path.basename(this._path);
        const ext = path.extname(base);
        return ext ? base.substring(0, base.length - ext.length) : base;
    }
    /**
     * Get the local filesystem path (converts URI to local path)
     */
    get local() {
        // Convert file:// URIs to local paths
        if (this._scheme === 'file') {
            return new Path(this._path);
        }
        return new Path(this._path);
    }
    /**
     * Rebuild the internal URL object when components change
     */
    _rebuildUrl() {
        if (this._scheme && this._host) {
            try {
                let urlStr = `${this._scheme}://${this._host}`;
                if (this._port)
                    urlStr += `:${this._port}`;
                urlStr += this._path || '/';
                if (this._query)
                    urlStr += `?${this._query}`;
                if (this._reference)
                    urlStr += `#${this._reference}`;
                this._url = new URL(urlStr);
            }
            catch {
                this._url = null;
            }
        }
        else {
            this._url = null;
        }
    }
    /**
     * Create an absolute URI from this URI with all components present
     * @param base Base URI to provide missing components
     * @returns Complete absolute URI
     */
    absolute(base = null) {
        if (this.isAbsolute) {
            return new Uri(this.toString());
        }
        const baseUri = base ? (typeof base === 'string' ? new Uri(base) : base) : new Uri('http://localhost/');
        return new Uri({
            scheme: this._scheme || baseUri.scheme,
            host: this._host || baseUri.host,
            port: this._port || baseUri.port,
            path: this._path ? (this._path.startsWith('/') ? this._path : path.join(baseUri.path, this._path)) : baseUri.path,
            query: this._query,
            reference: this._reference
        });
    }
    /**
     * Complete this URI with default values for missing components
     * @param missing URI providing default values
     * @returns Completed URI
     */
    complete(missing = null) {
        const defaults = missing ? (typeof missing === 'string' ? new Uri(missing) : missing) : new Uri('http://localhost:80/');
        return new Uri({
            scheme: this._scheme || defaults.scheme,
            host: this._host || defaults.host,
            port: this._port || defaults.port,
            path: this._path || defaults.path,
            query: this._query || defaults.query,
            reference: this._reference || defaults.reference
        });
    }
    /**
     * Normalize the URI path
     * @returns Normalized URI
     */
    normalize() {
        const normalized = new Uri(this.toString());
        normalized._path = path.normalize(this._path);
        normalized._rebuildUrl();
        return normalized;
    }
    /**
     * Make the URI relative by removing the scheme and host
     * @returns Relative URI
     */
    relative() {
        return new Uri(this._path);
    }
    /**
     * Make this URI relative to another URI
     * @param origin Base URI to make relative to
     * @returns Relative URI
     */
    relativeTo(origin = null) {
        if (!origin) {
            return this.relative();
        }
        const originUri = typeof origin === 'string' ? new Uri(origin) : origin;
        // If different schemes or hosts, can't make relative
        if (this._scheme !== originUri.scheme || this._host !== originUri.host) {
            return new Uri(this.toString());
        }
        // Make path relative
        const relativePath = path.relative(originUri.path, this._path);
        return new Uri(relativePath);
    }
    /**
     * Resolve other paths against this URI
     * @param otherPaths Paths to resolve
     * @returns Resolved URI
     */
    resolve(...otherPaths) {
        let resultPath = this._path;
        for (const other of otherPaths) {
            const otherStr = typeof other === 'string' ? other : other.path;
            resultPath = path.resolve(resultPath, otherStr);
        }
        const resolved = new Uri(this.toString());
        resolved._path = resultPath;
        resolved._rebuildUrl();
        return resolved;
    }
    /**
     * Join URI segments to this URI
     * @param segments Segments to join
     * @returns New URI with joined path
     */
    join(...segments) {
        const parts = [this._path];
        for (const segment of segments) {
            parts.push(typeof segment === 'string' ? segment : segment.path);
        }
        const joined = new Uri(this.toString());
        joined._path = path.join(...parts);
        joined._rebuildUrl();
        return joined;
    }
    /**
     * Join an extension to the URI path
     * @param ext Extension to add (with or without dot)
     * @param force If true, replace existing extension
     * @returns New URI with extension
     */
    joinExt(ext, force = false) {
        if (!ext.startsWith('.')) {
            ext = '.' + ext;
        }
        let newPath = this._path;
        if (force && this.hasExtension) {
            newPath = newPath.substring(0, newPath.lastIndexOf('.'));
        }
        newPath += ext;
        const result = new Uri(this.toString());
        result._path = newPath;
        result._rebuildUrl();
        return result;
    }
    /**
     * Replace the extension
     * @param ext New extension (with or without dot)
     * @returns New URI with replaced extension
     */
    replaceExt(ext) {
        if (!ext.startsWith('.')) {
            ext = '.' + ext;
        }
        let newPath = this._path;
        if (this.hasExtension) {
            newPath = newPath.substring(0, newPath.lastIndexOf('.'));
        }
        newPath += ext;
        const result = new Uri(this.toString());
        result._path = newPath;
        result._rebuildUrl();
        return result;
    }
    /**
     * Trim the extension from the path
     * @returns New URI without extension
     */
    trimExt() {
        if (!this.hasExtension) {
            return new Uri(this.toString());
        }
        const newPath = this._path.substring(0, this._path.lastIndexOf('.'));
        const result = new Uri(this.toString());
        result._path = newPath;
        result._rebuildUrl();
        return result;
    }
    /**
     * Trim a suffix from the path
     * @param suffix Suffix to remove
     * @returns New URI without suffix
     */
    trimEnd(suffix) {
        const newPath = this._path.endsWith(suffix) ? this._path.substring(0, this._path.length - suffix.length) : this._path;
        const result = new Uri(this.toString());
        result._path = newPath;
        result._rebuildUrl();
        return result;
    }
    /**
     * Trim a prefix from the path
     * @param prefix Prefix to remove
     * @returns New URI without prefix
     */
    trimStart(prefix) {
        const newPath = this._path.startsWith(prefix) ? this._path.substring(prefix.length) : this._path;
        const result = new Uri(this.toString());
        result._path = newPath;
        result._rebuildUrl();
        return result;
    }
    /**
     * Check if this URI is the same as another
     * @param other URI to compare
     * @returns True if URIs are the same
     */
    same(other) {
        const otherUri = typeof other === 'string' ? new Uri(other) : other;
        return this.toString() === otherUri.toString();
    }
    /**
     * Check if the URI path starts with a prefix
     * @param prefix Prefix to check
     * @returns True if path starts with prefix
     */
    startsWith(prefix) {
        const prefixStr = typeof prefix === 'string' ? prefix : prefix.path;
        return this._path.startsWith(prefixStr);
    }
    /**
     * Check if the URI path ends with a suffix
     * @param suffix Suffix to check
     * @returns True if path ends with suffix
     */
    endsWith(suffix) {
        const suffixStr = typeof suffix === 'string' ? suffix : suffix.path;
        return this._path.endsWith(suffixStr);
    }
    /**
     * Get MIME type based on file extension
     */
    get mimeType() {
        const ext = new Path(this._path).extension.toLowerCase();
        const mimeTypes = {
            'html': 'text/html',
            'htm': 'text/html',
            'css': 'text/css',
            'js': 'application/javascript',
            'mjs': 'application/javascript',
            'json': 'application/json',
            'xml': 'application/xml',
            'txt': 'text/plain',
            'png': 'image/png',
            'jpg': 'image/jpeg',
            'jpeg': 'image/jpeg',
            'gif': 'image/gif',
            'svg': 'image/svg+xml',
            'webp': 'image/webp',
            'ico': 'image/x-icon',
            'pdf': 'application/pdf',
            'zip': 'application/zip',
            'gz': 'application/gzip',
            'tar': 'application/x-tar',
            'mp3': 'audio/mpeg',
            'mp4': 'video/mp4',
            'webm': 'video/webm',
            'woff': 'font/woff',
            'woff2': 'font/woff2',
            'ttf': 'font/ttf',
            'eot': 'application/vnd.ms-fontobject'
        };
        return mimeTypes[ext] || 'application/octet-stream';
    }
    /**
     * Encode a full URI (encodes special chars but not delimiters like / ? #)
     * @param str String to encode
     * @returns Encoded string
     */
    static encodeURI(str) {
        return encodeURI(str);
    }
    /**
     * Decode a full URI
     * @param str String to decode
     * @returns Decoded string
     */
    static decodeURI(str) {
        return decodeURI(str);
    }
    /**
     * Encode a URI component (encodes all special chars including delimiters)
     * @param str String to encode
     * @returns Encoded string
     */
    static encodeURIComponent(str) {
        return encodeURIComponent(str);
    }
    /**
     * Decode a URI component
     * @param str String to decode
     * @returns Decoded string
     */
    static decodeURIComponent(str) {
        return decodeURIComponent(str);
    }
    /**
     * Alias for encodeURI (for compatibility)
     */
    static encode(str) {
        return Uri.encodeURI(str);
    }
    /**
     * Alias for decodeURI (for compatibility)
     */
    static decode(str) {
        return Uri.decodeURI(str);
    }
    /**
     * Encode an object or array as a query string
     * @param obj Object or array to encode
     * @returns Encoded query string (without '?')
     */
    static encodeObjects(obj) {
        if (Array.isArray(obj)) {
            return obj.map((item, i) => `${i}=${Uri.encodeURIComponent(String(item))}`).join('&');
        }
        const parts = [];
        for (const [key, value] of Object.entries(obj)) {
            if (value === null || value === undefined) {
                continue;
            }
            if (Array.isArray(value)) {
                for (const item of value) {
                    parts.push(`${Uri.encodeURIComponent(key)}=${Uri.encodeURIComponent(String(item))}`);
                }
            }
            else if (typeof value === 'object') {
                // Nested object - encode as JSON
                parts.push(`${Uri.encodeURIComponent(key)}=${Uri.encodeURIComponent(JSON.stringify(value))}`);
            }
            else {
                parts.push(`${Uri.encodeURIComponent(key)}=${Uri.encodeURIComponent(String(value))}`);
            }
        }
        return parts.join('&');
    }
    /**
     * Encode an object as query parameters (alias for encodeObjects)
     * @param obj Object to encode
     * @returns Query string (without '?')
     */
    static encodeQuery(obj) {
        return Uri.encodeObjects(obj);
    }
    /**
     * Decode a query string to an object
     * @param query Query string (with or without '?')
     * @returns Object with query parameters
     */
    static decodeQuery(query) {
        if (query.startsWith('?')) {
            query = query.substring(1);
        }
        const result = {};
        if (!query) {
            return result;
        }
        for (const part of query.split('&')) {
            const [key, value] = part.split('=');
            const decodedKey = Uri.decodeURIComponent(key);
            const decodedValue = value ? Uri.decodeURIComponent(value) : '';
            // Handle arrays (multiple values for same key)
            if (decodedKey in result) {
                if (Array.isArray(result[decodedKey])) {
                    result[decodedKey].push(decodedValue);
                }
                else {
                    result[decodedKey] = [result[decodedKey], decodedValue];
                }
            }
            else {
                result[decodedKey] = decodedValue;
            }
        }
        return result;
    }
    /**
     * Join URI segments
     * @param base Base URI
     * @param ...segments Segments to join
     * @returns New URI
     */
    static join(base, ...segments) {
        let result = base;
        for (const segment of segments) {
            if (!result.endsWith('/') && !segment.startsWith('/')) {
                result += '/';
            }
            result += segment;
        }
        return new Uri(result);
    }
    /**
     * Parse a URI string
     * @param uri URI string
     * @returns Parsed URI components
     */
    static parse(uri) {
        const u = new Uri(uri);
        return {
            scheme: u.scheme,
            host: u.host,
            port: u.port,
            path: u.path,
            query: u.query,
            hash: u.hash
        };
    }
    /**
     * Format URI components into a string
     * @param components URI components
     * @returns URI string
     */
    static format(components) {
        let result = '';
        if (components.scheme) {
            result += components.scheme + '://';
        }
        if (components.host) {
            result += components.host;
        }
        if (components.port) {
            result += ':' + components.port;
        }
        if (components.path) {
            result += components.path;
        }
        if (components.query) {
            result += '?' + components.query;
        }
        if (components.hash) {
            result += '#' + components.hash;
        }
        return result;
    }
}
//# sourceMappingURL=Uri.js.map