/**
 * FileSystem - File system operations
 *
 * Provides file system level operations and metadata
 * @spec ejs
 * @stability evolving
 */
import { Path } from './Path';
export declare class FileSystem {
    private path;
    /**
     * File system path separators
     */
    static readonly separators: string;
    /**
     * Default file system path separator for this platform
     */
    static readonly separator: string;
    /**
     * New line characters for this platform
     */
    static readonly newline: string;
    /**
     * Create a FileSystem object for the given path
     * @param path Path to examine
     */
    constructor(path: Path | string);
    /**
     * Get file system free space (in bytes)
     * Note: This is a simplified implementation
     */
    get freeSpace(): number;
    /**
     * Get total file system space (in bytes)
     */
    get totalSpace(): number;
    /**
     * Get file system type
     */
    get type(): string;
    /**
     * Check if the file system is writable
     */
    get writable(): boolean;
    /**
     * Check if the file system is readable
     */
    get readable(): boolean;
    /**
     * Get file system root
     */
    get root(): Path;
    /**
     * Check if path exists in file system
     */
    exists(): boolean;
    /**
     * Get path metadata
     */
    getMetadata(): {
        size: number;
        modified: Date | null;
        accessed: Date | null;
        created: Date | null;
        isDir: boolean;
        isFile: boolean;
        isLink: boolean;
        permissions: number | null;
    };
    /**
     * Create a directory
     * @param options Options including permissions
     */
    createDirectory(options?: {
        permissions?: number;
    }): Promise<boolean>;
    /**
     * Remove a file or directory
     */
    remove(): Promise<boolean>;
    /**
     * Remove directory and all contents
     */
    removeAll(): Promise<boolean>;
    /**
     * Get available file systems
     * This is platform-specific and simplified
     */
    static getFileSystems(): FileSystem[];
    /**
     * Get the file system for a path
     * @param path Path to examine
     */
    static getFileSystem(path: Path | string): FileSystem;
}
//# sourceMappingURL=FileSystem.d.ts.map