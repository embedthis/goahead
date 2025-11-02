/**
 * Path - File path manipulation and operations
 *
 * Paths represent files in a file system. The file may or may not exist.
 * Once created, Paths are immutable and their path value cannot be changed.
 * @spec ejs
 * @stability evolving
 */
export declare class Path {
    private _path;
    /**
     * Create a new Path object
     * @param pathString Path string (default: current directory)
     */
    constructor(pathString?: string);
    /**
     * Get the path string value
     */
    get name(): string;
    /**
     * Get an absolute path equivalent
     * The path is normalized and uses the native system directory separator
     */
    get absolute(): Path;
    /**
     * When the file was last accessed
     * Returns null if the file does not exist
     */
    get accessed(): Date | null;
    /**
     * File security attributes
     * @returns Object with user, group, uid, gid, permissions
     */
    get attributes(): {
        user?: string;
        group?: string;
        uid?: number;
        gid?: number;
        permissions?: number;
    };
    /**
     * Set file security attributes
     * @param attributes Object with optional user, group, uid, gid, permissions
     */
    setAttributes(attributes: {
        user?: string;
        group?: string;
        uid?: number;
        gid?: number;
        permissions?: number;
    }): void;
    /**
     * The base portion of the path (trailing portion without directory)
     */
    get basename(): Path;
    /**
     * Test if this path is a child of a given directory
     * @param dir Parent directory to test
     * @returns True if this path is a child of dir
     */
    childOf(dir: Path): boolean;
    /**
     * Path components array
     * The path is normalized and broken into components for each directory level
     */
    get components(): string[];
    /**
     * Test if the path contains a substring
     * @param pattern String pattern to search for (in portable format)
     * @returns True if found
     */
    contains(pattern: string): boolean;
    /**
     * Copy a file to the destination
     * @param destination New file location
     * @param options Options object (permissions, user, group)
     */
    copy(destination: Path | string, options?: {
        permissions?: number;
        user?: string;
        group?: string;
    }): Promise<void>;
    /**
     * When the file was created
     * Returns null if the file does not exist
     */
    get created(): Date | null;
    /**
     * The directory portion of the path
     * Includes all directory elements, excluding the base name
     */
    get dirname(): Path;
    /**
     * Test if the path ends with the given suffix
     * @param suffix String suffix to compare
     * @returns True if the path ends with suffix
     */
    endsWith(suffix: string): boolean;
    /**
     * Check if the file exists
     */
    get exists(): boolean;
    /**
     * File extension portion of the path
     * Returns the portion after (and not including) the last '.'
     */
    get extension(): string;
    /**
     * Find a file above the current directory
     * @param name File or directory name to find
     * @returns Path to found file, or null if not found
     */
    findAbove(name: string): Path | null;
    /**
     * Get a list of matching files using glob patterns
     * @param patterns Pattern or array of patterns
     * @param options Options for file matching
     * @returns Array of matching paths
     */
    files(patterns?: string | string[], options?: any): Path[];
    private _filesRecursive;
    /**
     * Check if path has a drive spec (Windows)
     */
    get hasDrive(): boolean;
    /**
     * Check if the path is absolute
     */
    get isAbsolute(): boolean;
    /**
     * Check if the path is a directory
     */
    get isDir(): boolean;
    /**
     * Check if the path is a symbolic link
     */
    get isLink(): boolean;
    /**
     * Check if the path is a regular file
     */
    get isRegular(): boolean;
    /**
     * Check if the path is relative
     */
    get isRelative(): boolean;
    /**
     * Join paths together
     * @param ...other Path segments to join
     * @returns New joined, normalized Path
     */
    join(...other: (string | Path)[]): Path;
    /**
     * Join an extension to a path
     * @param ext Extension to add
     * @param force Always add extension even if one exists
     * @returns New Path with extension
     */
    joinExt(ext: string, force?: boolean): Path;
    /**
     * Length of the path name in bytes
     */
    get length(): number;
    /**
     * Create a link to refer to this path
     * @param target Target path where link will be created
     * @param hard Create a hard link instead of symbolic
     */
    link(target: Path | string, hard?: boolean): void;
    /**
     * Get symbolic link target
     * Returns null if not a symbolic link
     */
    get linkTarget(): Path | null;
    /**
     * Create directory and all required intervening directories
     * @param options Options (permissions, user, group)
     * @returns True if successful or already exists
     */
    makeDir(options?: {
        permissions?: number;
        user?: string;
        group?: string;
    }): Promise<boolean>;
    /**
     * Create a temporary file
     * @returns New Path for the temp file
     */
    temp(): Path;
    /**
     * Map path directory separator
     * @param sep Separator to use
     * @returns New Path with mapped separators
     */
    map(sep?: string): Path;
    /**
     * Match the path to a regular expression
     * @param pattern Regular expression pattern
     * @returns Array of matching substrings
     */
    match(pattern: RegExp): RegExpMatchArray | null;
    /**
     * Get MIME type for the path's extension
     */
    get mimeType(): string;
    /**
     * When the file was last modified
     */
    get modified(): Date | null;
    /**
     * Normalized representation of the path
     */
    get normalize(): Path;
    /**
     * Natural (native) representation for the platform
     */
    get natural(): Path;
    /**
     * Parent directory of the path
     */
    get parent(): Path;
    /**
     * File permissions (Posix style)
     */
    get perms(): number | null;
    set perms(perms: number);
    /**
     * Portable (Unix-style) representation with '/' separators
     */
    get portable(): Path;
    /**
     * Path relative to current working directory
     */
    get relative(): Path;
    /**
     * Get path relative to an origin
     * @param origin Origin path (defaults to current directory)
     * @returns Relative path from origin to this path
     */
    relativeTo(origin?: Path | string | null): Path;
    /**
     * Remove the file or empty directory
     * @returns True if successful or doesn't exist
     */
    remove(): Promise<boolean>;
    /**
     * Remove directory and all its contents recursively
     * @returns True if successful
     */
    removeAll(): Promise<boolean>;
    /**
     * Rename/move the file
     * @param target New path
     */
    rename(target: Path | string): Promise<void>;
    /**
     * Replace the file extension
     * @param ext New extension
     * @returns New Path with replaced extension
     */
    replaceExt(ext: string): Path;
    /**
     * Search and replace in path
     * @param pattern Pattern to search for (string or RegExp)
     * @param replacement Replacement string or function
     * @returns New Path with replacements
     */
    replace(pattern: string | RegExp, replacement: string | ((match: string, ...args: any[]) => string)): Path;
    /**
     * Resolve paths in the neighborhood of this path
     * @param ...otherPaths Paths to resolve
     * @returns Resolved path
     */
    resolve(...otherPaths: (string | Path)[]): Path;
    /**
     * Root directory component of the path
     */
    get root(): Path;
    /**
     * Compare two paths to test if they represent the same file
     * @param other Other path to compare
     * @returns True if paths represent the same file
     */
    same(other: Path | string): boolean;
    /**
     * Path separator for this path
     */
    get separator(): string;
    /**
     * File size in bytes
     */
    get size(): number;
    /**
     * Test if path starts with the given prefix
     * @param prefix String prefix to compare
     * @returns True if path starts with prefix
     */
    startsWith(prefix: string): boolean;
    /**
     * Convert to JSON string
     */
    toJSON(): string;
    /**
     * Convert to lowercase
     */
    toLowerCase(): Path;
    /**
     * Convert to string
     */
    toString(): string;
    /**
     * Trim path components
     * @param count Number of components to trim (negative trims from end)
     * @returns Trimmed path
     */
    trimComponents(count: number): Path;
    /**
     * Trim pattern from end of path
     * @param pat Pattern to trim
     * @returns Path with pattern trimmed
     */
    trimEnd(pat: string): Path;
    /**
     * Trim file extension
     * @returns Path without extension
     */
    trimExt(): Path;
    /**
     * Trim pattern from start of path
     * @param pat Pattern to trim
     * @returns Path with pattern trimmed
     */
    trimStart(pat: string): Path;
    /**
     * Truncate the file to specified size
     * @param size New file size
     */
    truncate(size: number): Promise<void>;
    /**
     * Windows path representation
     */
    get windows(): Path;
    /**
     * Append data to a file
     * @param data Data to append
     * @param options File open options
     */
    append(data: string | Uint8Array, _options?: string): Promise<void>;
    /**
     * Write data to file (overwrites existing content)
     * @param ...args Data to write
     */
    write(...args: any[]): Promise<void>;
    /**
     * Read file contents as a ByteArray
     * @returns ByteArray containing file data
     */
    readBytes(): Promise<Uint8Array | null>;
    /**
     * Read file contents as a string
     * @returns File contents as string
     */
    readString(): Promise<string | null>;
    /**
     * Read file as JSON and deserialize
     * @returns Parsed JSON object
     */
    readJSON(): Promise<any>;
    /**
     * Read file as array of lines
     * @returns Array of lines
     */
    readLines(): Promise<string[] | null>;
    /**
     * Read file as XML
     * @returns XML object (simplified - returns parsed structure)
     */
    readXML(): Promise<any>;
    /**
     * Open a file and return a File object
     * @param options Open options (mode, permissions, etc.)
     * @returns File object
     * @example
     * const file = await new Path('/tmp/test.txt').open({ mode: 'w' })
     * await file.write('Hello World')
     * await file.close()
     */
    open(options?: any): Promise<any>;
    /**
     * Compact a path by removing '.' and '..' segments
     * @returns Compacted path
     */
    compact(): Path;
    /**
     * Test if path matches glob pattern
     * @param patterns Pattern or array of patterns
     * @returns True if matches
     */
    glob(patterns: string | string[]): boolean;
    /**
     * Create a link to this path
     * @param target Target path for the link
     * @param hard Create hard link if true, symbolic link if false
     */
    makeLink(target: Path | string, hard?: boolean): void;
    /**
     * Create a symbolic link (alias for makeLink with hard=false)
     * @param target Target path for the symbolic link
     */
    symlink(target: Path | string): void;
    /**
     * Create a temporary file or directory
     * @param template Template for temp name (e.g., 'tmp-XXXXXX')
     * @returns Path to created temp file/directory
     */
    static makeTemp(template?: string): Path;
    /**
     * Remove drive specifier from path (Windows)
     * @returns Path without drive
     */
    removeDrive(): Path;
    /**
     * Apply an operation to files matching a pattern
     * @param pattern Glob pattern
     * @param operation Function to apply to each matching file
     */
    operate(pattern: string, operation: (path: Path) => void): void;
    /**
     * Iterator for paths in a directory
     */
    [Symbol.iterator](): Iterator<Path>;
    /**
     * Open a text stream for reading or writing
     * @param mode Mode: 'r', 'w', 'a', 'rt', 'wt', 'at'
     * @returns TextStream instance
     */
    openTextStream(mode?: string): Promise<any>;
    /**
     * Open a binary stream for reading or writing
     * @param mode Mode: 'r', 'w', 'a'
     * @returns BinaryStream instance
     */
    openBinaryStream(mode?: string): Promise<any>;
}
//# sourceMappingURL=Path.d.ts.map