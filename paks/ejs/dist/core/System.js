/**
 * System - System utilities and information
 *
 * Provides methods to interact with the operating system
 * @spec ejs
 * @stability prototype
 */
import { hostname } from 'os';
import { tmpdir } from 'os';
import { networkInterfaces } from 'os';
export class System {
    /**
     * Default buffer size
     */
    static Bufsize = 1024;
    /**
     * Get the fully qualified system hostname
     * @returns The system hostname
     */
    static get hostname() {
        return hostname();
    }
    /**
     * Get the system IP address
     * Returns the first non-internal IPv4 address found
     * @returns The IP address or '127.0.0.1' if not found
     */
    static get ipaddr() {
        const nets = networkInterfaces();
        for (const name of Object.keys(nets)) {
            const netInfo = nets[name];
            if (!netInfo)
                continue;
            for (const net of netInfo) {
                // Skip internal (loopback) and non-IPv4 addresses
                if (net.family === 'IPv4' && !net.internal) {
                    return net.address;
                }
            }
        }
        return '127.0.0.1';
    }
    /**
     * Get the system temporary directory
     * @returns Path to the temporary directory
     */
    static get tmpdir() {
        return tmpdir();
    }
}
//# sourceMappingURL=System.js.map