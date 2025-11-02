/**
 * FileSystem - File system operations
 *
 * Provides file system level operations and metadata
 * @spec ejs
 * @stability evolving
 */
import { Path } from './Path';
import * as fs from 'fs';
import * as os from 'os';
export class FileSystem {
    path;
    /**
     * File system path separators
     */
    static separators = process.platform === 'win32' ? '\\/' : '/';
    /**
     * Default file system path separator for this platform
     */
    static separator = os.platform() === 'win32' ? '\\' : '/';
    /**
     * New line characters for this platform
     */
    static newline = os.EOL;
    /**
     * Create a FileSystem object for the given path
     * @param path Path to examine
     */
    constructor(path) {
        this.path = path instanceof Path ? path : new Path(path);
    }
    /**
     * Get file system free space (in bytes)
     * Note: This is a simplified implementation
     */
    get freeSpace() {
        // Bun/Node doesn't provide a built-in way to get disk space
        // Would need to use platform-specific commands
        return -1;
    }
    /**
     * Get total file system space (in bytes)
     */
    get totalSpace() {
        // Bun/Node doesn't provide a built-in way to get disk space
        return -1;
    }
    /**
     * Get file system type
     */
    get type() {
        return 'native';
    }
    /**
     * Check if the file system is writable
     */
    get writable() {
        try {
            fs.accessSync(this.path.name, fs.constants.W_OK);
            return true;
        }
        catch {
            return false;
        }
    }
    /**
     * Check if the file system is readable
     */
    get readable() {
        try {
            fs.accessSync(this.path.name, fs.constants.R_OK);
            return true;
        }
        catch {
            return false;
        }
    }
    /**
     * Get file system root
     */
    get root() {
        return this.path.root;
    }
    /**
     * Check if path exists in file system
     */
    exists() {
        return this.path.exists;
    }
    /**
     * Get path metadata
     */
    getMetadata() {
        return {
            size: this.path.size,
            modified: this.path.modified,
            accessed: this.path.accessed,
            created: this.path.created,
            isDir: this.path.isDir,
            isFile: this.path.isRegular,
            isLink: this.path.isLink,
            permissions: this.path.perms
        };
    }
    /**
     * Create a directory
     * @param options Options including permissions
     */
    async createDirectory(options) {
        return await this.path.makeDir(options);
    }
    /**
     * Remove a file or directory
     */
    async remove() {
        return await this.path.remove();
    }
    /**
     * Remove directory and all contents
     */
    async removeAll() {
        return await this.path.removeAll();
    }
    /**
     * Get available file systems
     * This is platform-specific and simplified
     */
    static getFileSystems() {
        // On Unix, return root
        // On Windows, would return drive letters
        if (process.platform === 'win32') {
            // Simplified - would enumerate drives
            return [new FileSystem('C:\\')];
        }
        else {
            return [new FileSystem('/')];
        }
    }
    /**
     * Get the file system for a path
     * @param path Path to examine
     */
    static getFileSystem(path) {
        return new FileSystem(path);
    }
}
//# sourceMappingURL=FileSystem.js.map