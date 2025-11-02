/**
 * Uri - URI parsing and manipulation
 *
 * Provides URI/URL parsing, encoding, and MIME type detection
 * @spec ejs
 * @stability evolving
 */
import { Path } from '../Path';
export declare class Uri {
    private _url;
    private _path;
    private _scheme;
    private _host;
    private _port;
    private _query;
    private _reference;
    /**
     * Create a Uri from a string or object
     * @param uri URI string or object with components
     */
    constructor(uri: string | object);
    /**
     * Get the complete URI as a string
     */
    toString(): string;
    /**
     * Get the scheme/protocol (e.g., 'http', 'https')
     */
    get scheme(): string | null;
    /**
     * Set the scheme/protocol
     */
    set scheme(value: string | null);
    /**
     * Get the host
     */
    get host(): string | null;
    /**
     * Set the host
     */
    set host(value: string | null);
    /**
     * Get the port
     */
    get port(): number | null;
    /**
     * Set the port
     */
    set port(value: number | null);
    /**
     * Get the path portion
     */
    get path(): string;
    /**
     * Set the path
     */
    set path(value: string);
    /**
     * Get the query string (without '?')
     */
    get query(): string | null;
    /**
     * Set the query string
     */
    set query(value: string | null);
    /**
     * Get the fragment/hash (without '#')
     */
    get hash(): string | null;
    /**
     * Get reference portion (same as hash)
     */
    get reference(): string | null;
    /**
     * Set the reference/hash
     */
    set reference(value: string | null);
    /**
     * Get the complete URI string
     */
    get uri(): string;
    /**
     * Get the TCP/IP address (host:port)
     */
    get address(): string;
    /**
     * Get all URI components as an object
     */
    get components(): {
        scheme: string | null;
        host: string | null;
        port: number | null;
        path: string;
        query: string | null;
        reference: string | null;
    };
    /**
     * Check if URI is absolute
     */
    get isAbsolute(): boolean;
    /**
     * Check if URI is relative
     */
    get isRelative(): boolean;
    /**
     * Check if URI has a scheme
     */
    get hasScheme(): boolean;
    /**
     * Check if URI has a host
     */
    get hasHost(): boolean;
    /**
     * Check if URI has a port
     */
    get hasPort(): boolean;
    /**
     * Check if URI has a query string
     */
    get hasQuery(): boolean;
    /**
     * Check if URI has a reference/hash
     */
    get hasReference(): boolean;
    /**
     * Check if path has an extension
     */
    get hasExtension(): boolean;
    /**
     * Check if path represents a directory (ends with /)
     */
    get isDir(): boolean;
    /**
     * Check if path represents a regular file (doesn't end with /)
     */
    get isRegular(): boolean;
    /**
     * Get the basename of the path
     */
    get basename(): Uri;
    /**
     * Get the directory name of the path
     */
    get dirname(): Uri;
    /**
     * Get the file extension (without dot)
     */
    get extension(): string;
    /**
     * Get the filename (basename without extension)
     */
    get filename(): string;
    /**
     * Get the local filesystem path (converts URI to local path)
     */
    get local(): Path;
    /**
     * Rebuild the internal URL object when components change
     */
    private _rebuildUrl;
    /**
     * Create an absolute URI from this URI with all components present
     * @param base Base URI to provide missing components
     * @returns Complete absolute URI
     */
    absolute(base?: Uri | string | null): Uri;
    /**
     * Complete this URI with default values for missing components
     * @param missing URI providing default values
     * @returns Completed URI
     */
    complete(missing?: Uri | string | null): Uri;
    /**
     * Normalize the URI path
     * @returns Normalized URI
     */
    normalize(): Uri;
    /**
     * Make the URI relative by removing the scheme and host
     * @returns Relative URI
     */
    relative(): Uri;
    /**
     * Make this URI relative to another URI
     * @param origin Base URI to make relative to
     * @returns Relative URI
     */
    relativeTo(origin?: Uri | string | null): Uri;
    /**
     * Resolve other paths against this URI
     * @param otherPaths Paths to resolve
     * @returns Resolved URI
     */
    resolve(...otherPaths: (Uri | string)[]): Uri;
    /**
     * Join URI segments to this URI
     * @param segments Segments to join
     * @returns New URI with joined path
     */
    join(...segments: (Uri | string)[]): Uri;
    /**
     * Join an extension to the URI path
     * @param ext Extension to add (with or without dot)
     * @param force If true, replace existing extension
     * @returns New URI with extension
     */
    joinExt(ext: string, force?: boolean): Uri;
    /**
     * Replace the extension
     * @param ext New extension (with or without dot)
     * @returns New URI with replaced extension
     */
    replaceExt(ext: string): Uri;
    /**
     * Trim the extension from the path
     * @returns New URI without extension
     */
    trimExt(): Uri;
    /**
     * Trim a suffix from the path
     * @param suffix Suffix to remove
     * @returns New URI without suffix
     */
    trimEnd(suffix: string): Uri;
    /**
     * Trim a prefix from the path
     * @param prefix Prefix to remove
     * @returns New URI without prefix
     */
    trimStart(prefix: string): Uri;
    /**
     * Check if this URI is the same as another
     * @param other URI to compare
     * @returns True if URIs are the same
     */
    same(other: Uri | string): boolean;
    /**
     * Check if the URI path starts with a prefix
     * @param prefix Prefix to check
     * @returns True if path starts with prefix
     */
    startsWith(prefix: Uri | string): boolean;
    /**
     * Check if the URI path ends with a suffix
     * @param suffix Suffix to check
     * @returns True if path ends with suffix
     */
    endsWith(suffix: Uri | string): boolean;
    /**
     * Get MIME type based on file extension
     */
    get mimeType(): string;
    /**
     * Encode a full URI (encodes special chars but not delimiters like / ? #)
     * @param str String to encode
     * @returns Encoded string
     */
    static encodeURI(str: string): string;
    /**
     * Decode a full URI
     * @param str String to decode
     * @returns Decoded string
     */
    static decodeURI(str: string): string;
    /**
     * Encode a URI component (encodes all special chars including delimiters)
     * @param str String to encode
     * @returns Encoded string
     */
    static encodeURIComponent(str: string): string;
    /**
     * Decode a URI component
     * @param str String to decode
     * @returns Decoded string
     */
    static decodeURIComponent(str: string): string;
    /**
     * Alias for encodeURI (for compatibility)
     */
    static encode(str: string): string;
    /**
     * Alias for decodeURI (for compatibility)
     */
    static decode(str: string): string;
    /**
     * Encode an object or array as a query string
     * @param obj Object or array to encode
     * @returns Encoded query string (without '?')
     */
    static encodeObjects(obj: Record<string, any> | any[]): string;
    /**
     * Encode an object as query parameters (alias for encodeObjects)
     * @param obj Object to encode
     * @returns Query string (without '?')
     */
    static encodeQuery(obj: Record<string, any>): string;
    /**
     * Decode a query string to an object
     * @param query Query string (with or without '?')
     * @returns Object with query parameters
     */
    static decodeQuery(query: string): Record<string, any>;
    /**
     * Join URI segments
     * @param base Base URI
     * @param ...segments Segments to join
     * @returns New URI
     */
    static join(base: string, ...segments: string[]): Uri;
    /**
     * Parse a URI string
     * @param uri URI string
     * @returns Parsed URI components
     */
    static parse(uri: string): {
        scheme: string | null;
        host: string | null;
        port: number | null;
        path: string;
        query: string | null;
        hash: string | null;
    };
    /**
     * Format URI components into a string
     * @param components URI components
     * @returns URI string
     */
    static format(components: {
        scheme?: string;
        host?: string;
        port?: number;
        path?: string;
        query?: string;
        hash?: string;
    }): string;
}
//# sourceMappingURL=Uri.d.ts.map